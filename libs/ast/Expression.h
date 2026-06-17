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

    TypeNode_ptr declared_type;

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

    Symbol_ptr symbol = nullptr;
    bool must_be_captured = false;
};

struct MemberAccess
{
    Expression_ptr owner;
    Expression_ptr member;

    int member_index = -1;
};

struct Call
{
    Expression_ptr callee;
    TypeNodeVector angular_nodes;
    ExpressionVector arguments;

    enum class OwnerKind
    {
        IDK,

        CLASS,
        TRAIT,
        PRIMITIVE
    } owner_kind = OwnerKind::IDK;

    enum class Kind
    {
        FREE,
        INSTANCE,
        STATIC
    } kind = Kind::FREE;

    int overload_index = -1;
    int owner_type_id = -1;
};

struct Constructor
{
    Expression_ptr constructible;
    TypeNodeVector angular_nodes;
    ExpressionVector arguments;
};

struct Range
{
    Expression_ptr start;
    Expression_ptr end;

    bool is_inclusive;
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
