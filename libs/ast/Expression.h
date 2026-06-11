#pragma once

#include "AST.h"
#include "Token.h"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>

namespace Wasp
{

struct IntegerLiteral
{
    int value;
};

struct FloatLiteral
{
    double value;
};

struct StringLiteral
{
    std::string value;
};

struct BooleanLiteral
{
    bool value;
};

struct NoneLiteral
{
};

struct InterpolatedString
{
    ExpressionVector parts;
};

struct Prefix
{
    Token op;
    Expression_ptr operand;
};

struct Infix
{
    Expression_ptr left;
    Token op;
    Expression_ptr right;
};

struct SequenceLiteral
{
    ExpressionVector expressions;
};

struct ListLiteral : public SequenceLiteral
{
    using SequenceLiteral::SequenceLiteral;
};

struct TupleLiteral : public SequenceLiteral
{
    using SequenceLiteral::SequenceLiteral;
};

struct SetLiteral : public SequenceLiteral
{
    using SequenceLiteral::SequenceLiteral;
};

struct MapLiteral
{
    std::map<Expression_ptr, Expression_ptr> pairs;
};

struct Assignment
{
    Expression_ptr lhs;
    Expression_ptr rhs;

    std::optional<TypeAnnotation_ptr> declared_type = std::nullopt;

    bool is_definition = false;
    bool is_mutable = false;
};

// Branching

struct TernaryExpression
{
    Expression_ptr then_expr;
    Expression_ptr test;
    Expression_ptr else_expr;
};

// Identifiers & Access

struct Identifier
{
    std::string name;
};

struct MemberAccess
{
    Expression_ptr object;
    std::string member_name;
};

struct Call
{
    Expression_ptr callable;
    ExpressionVector arguments;
};

struct Constructor
{
    TypeAnnotation_ptr type;
    ExpressionVector arguments;
};

struct Range
{
    Expression_ptr start;
    Expression_ptr end;

    enum class Kind
    {
        Inclusive,
        ExclusiveEnd,
        ExclusiveStart
    } kind;
};

struct Pipe
{
    Expression_ptr left;
    Expression_ptr right;
};

// Expression

using ExpressionVariant = std::variant<
    std::monostate,

    IntegerLiteral,
    FloatLiteral,
    StringLiteral,
    BooleanLiteral,
    NoneLiteral,
    InterpolatedString,
    Range,

    Identifier,
    MemberAccess,

    Call,
    Pipe,
    Constructor,

    Prefix,
    Infix,

    ListLiteral,
    TupleLiteral,
    MapLiteral,
    SetLiteral,

    Assignment,

    TernaryExpression>;

struct Expression : public AstNode<ExpressionVariant>
{
    using AstNode::AstNode;
    bool is_desugared = false;
};

template <typename T> inline Expression_ptr make_expression(T&& data, bool is_desugared = false)
{
    auto expr = std::make_shared<Expression>(std::forward<T>(data));
    expr->is_desugared = is_desugared;
    return expr;
}

inline std::string get_operator_name(TokenType fixity, TokenType op_type)
{
    std::string fix;

    if (fixity == TokenType::INFIX)
    {
        fix = "infix_";
    }
    if (fixity == TokenType::PREFIX)
    {
        fix = "prefix_";
    }

    return fix + to_string(op_type);
}

} // namespace Wasp
