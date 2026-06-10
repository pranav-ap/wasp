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

    IntegerLiteral() = default;

    explicit IntegerLiteral(int value) : value(value)
    {
    }
};

struct FloatLiteral
{
    double value;

    FloatLiteral() = default;

    explicit FloatLiteral(double value) : value(value)
    {
    }
};

struct StringLiteral
{
    std::string value;

    StringLiteral() = default;

    explicit StringLiteral(std::string value) : value(std::move(value))
    {
    }
};

struct BooleanLiteral
{
    bool value;

    BooleanLiteral() = default;

    explicit BooleanLiteral(bool value) : value(value)
    {
    }
};

struct NoneLiteral
{
};

struct InterpolatedString
{
    ExpressionVector parts;
};

struct Box
{
    Expression_ptr expr;

    Box() = default;

    Box(Expression_ptr expr) : expr(std::move(expr))
    {
    }
};

struct OperatorExpression
{
    OperatorExpression() = default;
};

struct Prefix : public OperatorExpression
{
    Token op;
    Expression_ptr operand;

    Prefix() = default;

    Prefix(Token op, Expression_ptr operand)
        : op(std::move(op)), operand(std::move(operand))
    {
    }
};

struct Infix : public OperatorExpression
{
    Expression_ptr left;
    Token op;
    Expression_ptr right;

    Infix() = default;

    Infix(Expression_ptr left, Token op, Expression_ptr right)
        : left(std::move(left)), op(std::move(op)), right(std::move(right))
    {
    }
};

struct SequenceLiteral
{
    ExpressionVector expressions;

    SequenceLiteral() = default;
    explicit SequenceLiteral(ExpressionVector expressions)
        : expressions(std::move(expressions)) {};
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

    explicit MapLiteral() = default;
    explicit MapLiteral(std::map<Expression_ptr, Expression_ptr> pairs)
        : pairs(std::move(pairs)) {};
};

struct Assignment
{
    Expression_ptr lhs;
    Expression_ptr rhs;

    std::optional<TypeAnnotation_ptr> declared_type = std::nullopt;

    bool is_definition = false;
    bool is_mutable = false;

    Assignment() = default;

    Assignment(Expression_ptr lhs, Expression_ptr rhs)
        : lhs(std::move(lhs)), rhs(std::move(rhs)), is_definition(false),
          is_mutable(false)
    {
    }

    Assignment(
        Expression_ptr lhs,
        Expression_ptr rhs,
        bool is_definition,
        bool is_mutable,
        std::optional<TypeAnnotation_ptr> declared_type = std::nullopt
    )
        : lhs(std::move(lhs)), rhs(std::move(rhs)),
          declared_type(std::move(declared_type)), is_definition(is_definition),
          is_mutable(is_mutable)
    {
    }
};

// Branching

struct TernaryBranch
{
};

struct IfTernaryBranch : public TernaryBranch
{
    Expression_ptr test;
    Expression_ptr true_expression;

    // IfTernaryBranch or ElseTernaryBranch
    Expression_ptr alternative;

    IfTernaryBranch() = default;

    IfTernaryBranch(
        Expression_ptr test,
        Expression_ptr true_expression,
        Expression_ptr alternative
    )
        : test(test), true_expression(true_expression),
          alternative(alternative) {};
};

struct ElseTernaryBranch : public TernaryBranch
{
    Expression_ptr expression;

    ElseTernaryBranch() = default;

    ElseTernaryBranch(Expression_ptr expression) : expression(expression) {};
};

// Identifiers & Access

struct Identifier
{
    std::string name;

    Identifier() = default;

    Identifier(std::string name) : name(std::move(name))
    {
    }
};

struct MemberAccess
{
    Expression_ptr left;
    Expression_ptr right;

    MemberAccess() = default;

    MemberAccess(Expression_ptr left, Expression_ptr right)
        : left(std::move(left)), right(std::move(right))
    {
    }
};

struct Call
{
    Expression_ptr callable;
    ExpressionVector arguments;

    Call() = default;

    Call(Expression_ptr callable, ExpressionVector arguments)
        : callable(callable), arguments(std::move(arguments))
    {
    }
};

struct Constructor
{
    Expression_ptr construtable;
    ExpressionVector values;

    Constructor() = default;

    Constructor(Expression_ptr construtable, ExpressionVector values)
        : construtable(construtable), values(std::move(values))
    {
    }
};

// Foo<int>
struct TemplateAngular
{
    Expression_ptr target;
    TypeAnnotationVector angular_nodes;

    TemplateAngular(Expression_ptr target, TypeAnnotationVector angular_nodes)
        : target(std::move(target)), angular_nodes(std::move(angular_nodes))
    {
    }
};

// Expression

using ExpressionVariant = std::variant<
    std::monostate,

    IntegerLiteral,
    FloatLiteral,
    StringLiteral,
    BooleanLiteral,

    InterpolatedString,

    NoneLiteral,

    Box,

    Identifier,

    MemberAccess,

    Call,

    Constructor,
    TemplateAngular,

    Prefix,
    Infix,

    ListLiteral,
    TupleLiteral,
    MapLiteral,
    SetLiteral,

    Assignment,

    IfTernaryBranch,
    ElseTernaryBranch>;

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
