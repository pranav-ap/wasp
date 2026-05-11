#include "AST.h"
#include "Compiler.h"
#include "Doctor.h"
#include "Expression.h"
#include "OpCode.h"
#include "Token.h"
#include "Workspace.h"

#include <memory>
#include <string>
#include <variant>
#include <vector>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

void Compiler::visit(Identifier& expr)
{
    auto symbol = expr.symbol;
    Doctor::get().fatal_if_nullptr(symbol, WaspStage::Compiler);

    if (symbol->is_native())
    {
        std::string mangled = mangle_name(
            symbol->name,
            "",
            symbol->module_path
        );

        auto id = workspace->native_registry->get_native_index(mangled);
        emit(OpCode::GET_NATIVE, id, mangled);
    }
    else if (expr.must_be_captured)
    {
        int upval_index = resolve_upvalue(this, symbol);
        emit(OpCode::GET_UPVALUE, upval_index, symbol->name);
    }
    else
    {
        int stack_index = resolve_local(symbol->id);

        Doctor::get().assert(
            stack_index != -1,
            WaspStage::Compiler,
            "Attempted to read an unresolved local variable: " + symbol->name
        );

        emit(OpCode::GET_LOCAL, stack_index, symbol->name);
    }
}

void Compiler::visit(MemberAccess& access)
{
    if (access.is_enum_value)
    {
        int const_id = workspace->pool->allocate(access.member_index);
        emit(OpCode::LOAD_CONST, const_id);
        return;
    }

    visit(access.left);
    emit(OpCode::GET_MEMBER, access.member_index);
}

void Compiler::compile_member_assignment(
    MemberAccess& access,
    const Expression_ptr& value
)
{
    visit(access.left);
    visit(value);

    Doctor::get().assert(
        access.right->is<Identifier>(),
        WaspStage::Compiler,
        "Right side of member assignment must be an Identifier"
    );

    auto target_name = access.right->as<Identifier>().name;
    emit(OpCode::SET_MEMBER, access.member_index, target_name);
}

void Compiler::compile_identifier_assignment(
    Identifier& id,
    const Expression_ptr& rhs
)
{
    visit(rhs);

    auto symbol = id.symbol;
    Doctor::get().fatal_if_nullptr(symbol, WaspStage::Compiler);

    if (id.must_be_captured)
    {
        int idx = resolve_upvalue(this, symbol);
        emit(OpCode::SET_UPVALUE, idx, symbol->name);
    }
    else
    {
        int stack_index = resolve_local(symbol->id);

        Doctor::get().assert(
            stack_index != -1,
            WaspStage::Compiler,
            "Attempted to assign to an unresolved local variable: " +
                symbol->name
        );

        emit(OpCode::SET_LOCAL, stack_index, symbol->name);
    }
}

void Compiler::visit(Call& expr)
{
    visit(expr.callable);

    int resolve_idx = expr.overload_index == -1 ? 0 : expr.overload_index;

    emit(OpCode::RESOLVE_FUNCTION, resolve_idx);

    int total_arguments = static_cast<int>(expr.arguments.size());

    if (expr.is_method_call)
    {
        auto& mac = expr.callable->as<MemberAccess>();
        visit(mac.left);
        total_arguments++;
    }

    for (const auto& arg : expr.arguments)
    {
        visit(arg);
    }

    emit(OpCode::CALL, total_arguments);
}

void Compiler::visit(Constructor& expr)
{
    for (const auto& arg : expr.values)
    {
        visit(arg);
    }

    visit(expr.construtable);
    emit(OpCode::INSTANTIATE, static_cast<int>(expr.values.size()));
}

void Compiler::visit(TemplateAngular& expr)
{
    visit(expr.target);
}

void Compiler::visit(std::vector<Expression_ptr>& expressions)
{
    for (const auto& expr : expressions)
    {
        visit(expr);
    }
}

void Compiler::visit(const Expression_ptr expr)
{
    Doctor::get().fatal_if_nullptr(expr, WaspStage::Compiler);

    std::visit(
        [this](auto& node)
        {
            if constexpr (requires { this->visit(node); })
            {
                this->visit(node);
            }
            else
            {
                Doctor::get().fatal(
                    WaspStage::Compiler,
                    "Unimplemented expression compilation"
                );
            }
        },
        expr->data
    );
}

void Compiler::visit(IntegerLiteral& expr)
{
    emit(
        OpCode::LOAD_CONST,
        workspace->pool->allocate(expr.value),
        std::to_string(expr.value)
    );
}

void Compiler::visit(FloatLiteral& expr)
{
    emit(
        OpCode::LOAD_CONST,
        workspace->pool->allocate(expr.value),
        std::to_string(expr.value)
    );
}

void Compiler::visit(StringLiteral& expr)
{
    emit(OpCode::LOAD_CONST, workspace->pool->allocate(expr.value), expr.value);
}

void Compiler::visit(BooleanLiteral& expr)
{
    emit(expr.value ? OpCode::LOAD_TRUE : OpCode::LOAD_FALSE);
}

void Compiler::visit(UntypedAssignment& expr)
{
    Doctor::get().fatal_if_nullptr(expr.lhs_expression, WaspStage::Compiler);

    std::visit(
        overloaded{
            [&](Identifier& id)
            {
                compile_identifier_assignment(id, expr.rhs_expression);
            },
            [&](MemberAccess& mac)
            {
                compile_member_assignment(mac, expr.rhs_expression);
            },
            [&](auto&)
            {
                Doctor::get().fatal(
                    WaspStage::Compiler,
                    "Invalid left-hand side for assignment. Must be an "
                    "Identifier or MemberAccess."
                );
            }
        },
        expr.lhs_expression->data
    );
}

void Compiler::visit(TypedAssignment& expr)
{
}

void Compiler::visit(Prefix& expr)
{
    visit(expr.operand);

    switch (expr.op.type)
    {
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

void Compiler::visit(Infix& expr)
{
    visit(expr.left);
    visit(expr.right);

    switch (expr.op.type)
    {
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
    case TokenType::MOD:
        emit(OpCode::MOD);
        break;
    case TokenType::EQUAL_EQUAL:
        emit(OpCode::EQ);
        break;
    case TokenType::BANG_EQUAL:
        emit(OpCode::NE);
        break;
    case TokenType::GREATER_THAN:
        emit(OpCode::GT);
        break;
    case TokenType::GREATER_THAN_EQUAL:
        emit(OpCode::GE);
        break;
    case TokenType::LESSER_THAN:
        emit(OpCode::LT);
        break;
    case TokenType::LESSER_THAN_EQUAL:
        emit(OpCode::LE);
        break;
    default:
        Doctor::get().fatal(
            WaspStage::Compiler,
            "Unsupported infix operator in compiler: " + to_string(expr.op.type)
        );
        break;
    }
}

void Compiler::visit(ListLiteral& expr)
{
    visit(expr.expressions);
    emit(OpCode::BUILD_LIST, static_cast<int>(expr.expressions.size()));
}

void Compiler::visit(TupleLiteral& expr)
{
    visit(expr.expressions);
    emit(OpCode::BUILD_TUPLE, static_cast<int>(expr.expressions.size()));
}

void Compiler::visit(SetLiteral& expr)
{
    visit(expr.expressions);
    emit(OpCode::BUILD_SET, static_cast<int>(expr.expressions.size()));
}

void Compiler::visit(MapLiteral& expr)
{
    for (auto& [k, v] : expr.pairs)
    {
        visit(k);
        visit(v);
    }
    emit(OpCode::BUILD_MAP, static_cast<int>(expr.pairs.size()));
}

void Compiler::visit(RangeLiteral& expr)
{
    if (expr.start)
    {
        visit(expr.start);
    }
    else
    {
        emit(OpCode::LOAD_NONE);
    }
    if (expr.end)
    {
        visit(expr.end);
    }
    else
    {
        emit(OpCode::LOAD_NONE);
    }
    if (expr.step)
    {
        visit(expr.step);
    }
    else
    {
        emit(OpCode::LOAD_NONE);
    }

    emit(OpCode::BUILD_RANGE, expr.is_inclusive ? 1 : 0);
}

void Compiler::visit(DotLiteral& expr)
{
}

void Compiler::visit(TypePattern& expr)
{
}

void Compiler::visit(Postfix& expr)
{
}

} // namespace Wasp
