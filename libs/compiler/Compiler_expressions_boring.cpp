#include "AST.h"
#include "Compiler.h"
#include "Doctor.h"
#include "Expression.h"
#include "OpCode.h"
#include "Token.h"

#include <map>
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
// -----------------------------------------------------------------------
// Expressions
// -----------------------------------------------------------------------

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

void Compiler::visit(int expr)
{
    emit(
        OpCode::LOAD_CONST,
        workspace->pool->allocate(expr),
        std::to_string(expr)
    );
}

void Compiler::visit(double expr)
{
    emit(OpCode::LOAD_CONST, workspace->pool->allocate(expr), std::to_string(expr));
}

void Compiler::visit(std::string expr)
{
    emit(OpCode::LOAD_CONST, workspace->pool->allocate(expr), expr);
}

void Compiler::visit(bool expr)
{
    emit(expr ? OpCode::LOAD_TRUE : OpCode::LOAD_FALSE);
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
                    "Invalid left-hand side for assignment. Must be an Identifier or "
                    "MemberAccess."
                );
            }
        },
        expr.lhs_expression->data
    );
}

void Compiler::visit(TypedAssignment& expr)
{
    // do nothing
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
