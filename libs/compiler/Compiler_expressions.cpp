#include "AST.h"
#include "Compiler.h"
#include "Doctor.h"
#include "Expression.h"
#include "OpCode.h"
#include "Symbol.h"
#include "Token.h"

#include <map>
#include <memory>
#include <string>
#include <variant>
#include <vector>

template <class... Ts> struct overloaded : Ts... {
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp {
// -----------------------------------------------------------------------
// Expressions
// -----------------------------------------------------------------------

void Compiler::visit(std::vector<Expression_ptr>& expressions) {
    for (const auto& expr : expressions) {
        visit(expr);
    }
}

void Compiler::visit(const Expression_ptr expr) {
    Doctor::get().fatal_if_nullptr(expr, WaspStage::Compiler);

    std::visit(
        overloaded{
            [&](int val) { visit(val); },
            [&](double val) { visit(val); },
            [&](std::string val) { visit(val); },
            [&](bool val) { visit(val); },
            [&](UntypedAssignment& a) { visit(a); },
            [&](TypedAssignment& a) { visit(a); },
            [&](Prefix& p) { visit(p); },
            [&](Infix& i) { visit(i); },
            [&](Identifier& id) { visit(id); },
            [&](MemberAccess& m) { visit(m); },
            [&](Call& c) { visit(c); },
            [&](ListLiteral& l) { visit(l); },
            [&](TupleLiteral& t) { visit(t); },
            [&](MapLiteral& m) { visit(m); },
            [&](SetLiteral& s) { visit(s); },
            [&](RangeLiteral& r) { visit(r); },
            [&](VariableDefinitionExpression& v) { visit(v); },
            [&](IfTernaryBranch& i) { visit(i); },
            [&](ElseTernaryBranch& e) { visit(e); },
            [&](auto&) { /* Fallback */ }},
        expr->data);
}

void Compiler::visit(int expr) { emit(OpCode::LOAD_CONST, pool->allocate(expr)); }

void Compiler::visit(double expr) { emit(OpCode::LOAD_CONST, pool->allocate(expr)); }

void Compiler::visit(std::string expr) { emit(OpCode::LOAD_CONST, pool->allocate(expr)); }

void Compiler::visit(bool expr) { emit(expr ? OpCode::LOAD_TRUE : OpCode::LOAD_FALSE); }

void Compiler::visit(Identifier& expr) {
    auto symbol = expr.symbol;
    Doctor::get().fatal_if_nullptr(symbol, WaspStage::Compiler);

    if (symbol->payload_is<FunctionData>() && symbol->get_payload_as<FunctionData>().is_native) {
        auto native_registry_id = native_registry->get_native_index(symbol->name);
        emit(OpCode::GET_NATIVE, native_registry_id);
    } else if (expr.must_be_captured) {
        int upval_index = resolve_upvalue(this, symbol);
        emit(OpCode::GET_UPVALUE, upval_index);
    } else {
        emit(OpCode::GET_LOCAL, symbol->id);
    }
}

void Compiler::visit(Call& expr)
{
    visit(expr.callable);

    for (const auto& arg : expr.arguments)
        visit(arg);

    emit(OpCode::CALL, static_cast<int>(expr.arguments.size()));
}

void Compiler::visit(MemberAccess& expr)
{
    visit(expr.left);

    auto& right_id = expr.right->as<Identifier>();
    std::string member_name = right_id.name;

    int name_index = pool->allocate(member_name);
    emit(OpCode::GET_MEMBER, name_index);
}

void Compiler::compile_identifier_assignment(Identifier& id, const Expression_ptr& rhs) {
    visit(rhs);

    auto symbol = id.symbol;
    Doctor::get().fatal_if_nullptr(symbol, WaspStage::Compiler);

    if (id.must_be_captured) {
        int idx = resolve_upvalue(this, symbol);
        emit(OpCode::SET_UPVALUE, idx);
    } else {
        emit(OpCode::SET_LOCAL, symbol->id);
    }
}

void Compiler::compile_member_assignment(MemberAccess& mac, const Expression_ptr& rhs)
{
    // Evaluate the object first (Stack: [obj])
    visit(mac.left);

    // Evaluate the value second (Stack: [obj, val])
    visit(rhs);

    // Extract the property name
    Doctor::get().assert(
        mac.right->is<Identifier>(),
        WaspStage::Compiler,
        "Right side of member assignment must be an Identifier"
    );

    std::string member_name = mac.right->as<Identifier>().name;
    int name_index = pool->allocate(member_name);
    emit(OpCode::SET_MEMBER, name_index);
}

void Compiler::visit(UntypedAssignment& expr) {
    Doctor::get().fatal_if_nullptr(expr.lhs_expression, WaspStage::Compiler);

    std::visit(
        overloaded{
            [&](Identifier& id) { compile_identifier_assignment(id, expr.rhs_expression); },
            [&](MemberAccess& mac) { compile_member_assignment(mac, expr.rhs_expression); },
            [&](auto&)
            {
                Doctor::get().fatal(
                    WaspStage::Compiler,
                    "Invalid left-hand side for assignment. Must be an Identifier or MemberAccess."
                );
            }},
        expr.lhs_expression->data);
}

void Compiler::visit(TypedAssignment& expr) {
    // do nothing
}

void Compiler::visit(Prefix& expr) {
    visit(expr.operand);

    switch (expr.op.type) {
    case TokenType::MINUS:
        emit(OpCode::NEGATE);
        break;
    case TokenType::NOT:
        emit(OpCode::NOT);
        break;
    default:
        break;
    }
}

void Compiler::visit(Infix& expr) {
    visit(expr.left);
    visit(expr.right);

    switch (expr.op.type) {
    case TokenType::PLUS:
        emit(OpCode::ADD);
        break;
    case TokenType::MINUS:
        emit(OpCode::SUB);
        break;
    case TokenType::STAR:
        emit(OpCode::MUL);
        break;
    case TokenType::DIVISION:
        emit(OpCode::DIV);
        break;
    case TokenType::EQUAL_EQUAL:
        emit(OpCode::EQ);
        break;
    default:
        break;
    }
}

void Compiler::visit(ListLiteral& expr) {
    visit(expr.expressions);
    emit(OpCode::BUILD_LIST, (int)expr.expressions.size());
}

void Compiler::visit(TupleLiteral& expr) {
    visit(expr.expressions);
    emit(OpCode::BUILD_TUPLE, (int)expr.expressions.size());
}

void Compiler::visit(SetLiteral& expr) {
    visit(expr.expressions);
    emit(OpCode::BUILD_SET, (int)expr.expressions.size());
}

void Compiler::visit(MapLiteral& expr) {
    for (auto& [k, v] : expr.pairs) {
        visit(k);
        visit(v);
    }

    emit(OpCode::BUILD_MAP, (int)expr.pairs.size());
}

void Compiler::visit(RangeLiteral& expr) {
    if (expr.start)
        visit(expr.start);
    else
        emit(OpCode::LOAD_NONE);
    if (expr.end)
        visit(expr.end);
    else
        emit(OpCode::LOAD_NONE);
    if (expr.step)
        visit(expr.step);
    else
        emit(OpCode::LOAD_NONE);

    emit(OpCode::BUILD_RANGE, expr.is_inclusive ? 1 : 0);
}

void Compiler::visit(DotLiteral& expr) {}

void Compiler::visit(TypePattern& expr) {}

void Compiler::visit(Postfix& expr) {}

} // namespace Wasp
