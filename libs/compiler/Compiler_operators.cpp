#include "Compiler.h"
#include "AST.h"
#include "Doctor.h"
#include "Expression.h"
#include "OpCode.h"
#include "Token.h"


template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

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
    case TokenType::POWER:
        emit(OpCode::POW);
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
    case TokenType::AND:
        emit(OpCode::LOGICAL_AND);
        break;
    case TokenType::OR:
        emit(OpCode::LOGICAL_OR);
        break;
    default:
        Doctor::get().fatal(
            WaspStage::Compiler,
            "Unsupported infix operator in compiler: " + to_string(expr.op.type)
        );
        break;
    }
}

void Compiler::visit(Postfix& expr)
{
}

} // namespace Wasp
