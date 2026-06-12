#pragma once

#include "AST.h"
#include "Token.h"

#include <map>
#include <memory>
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

struct ListLiteral
{
    ExpressionVector expressions;
};

struct TupleLiteral
{
    ExpressionVector expressions;
};

struct SetLiteral
{
    ExpressionVector expressions;
};

struct MapLiteral
{
    std::map<Expression_ptr, Expression_ptr> pairs;
};

// Binding & Assignment

struct Binding
{
    Expression_ptr lhs;
    Expression_ptr rhs;

    TypeAnnotation_ptr declared_type;

    bool is_mutable;
};

struct Assignment
{
    Expression_ptr lhs;
    Expression_ptr rhs;
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
    Expression_ptr member;
};

struct Call
{
    Expression_ptr callable;
    ExpressionVector arguments;
};

struct Constructor
{
    Expression_ptr constructible;
    ExpressionVector arguments;
};

struct Range
{
    Expression_ptr start;
    Expression_ptr end;

    enum class Kind
    {
        Inclusive,
        Exclusive,
    } kind;
};

struct Pipe
{
    Expression_ptr left;
    Expression_ptr right;
};

struct TemplateAngular
{
    Expression_ptr target;
    TypeAnnotationVector angular_nodes;
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
    TemplateAngular,

    Call,
    Pipe,
    Constructor,

    Prefix,
    Infix,

    ListLiteral,
    TupleLiteral,
    MapLiteral,
    SetLiteral,

    Binding,
    Assignment,

    TernaryExpression>;

struct Expression : public AstNode<ExpressionVariant>
{
    using AstNode::AstNode;
};

template <typename T> inline Expression_ptr make_expression(T&& data)
{
    auto expr = std::make_shared<Expression>(std::forward<T>(data));
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
