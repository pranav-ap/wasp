#pragma once

#include "AST.h"
#include "Resolvable.h"
#include "Token.h"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>

namespace Wasp
{

struct BoxableLiteral : public Resolvable
{
    Expression_ptr construtable = nullptr;

    BoxableLiteral() = default;
};

struct IntegerLiteral : public BoxableLiteral
{
    int value;

    IntegerLiteral() = default;

    explicit IntegerLiteral(int value) : value(value)
    {
    }
};

struct FloatLiteral : public BoxableLiteral
{
    double value;

    FloatLiteral() = default;

    explicit FloatLiteral(double value) : value(value)
    {
    }
};

struct StringLiteral : public BoxableLiteral
{
    std::string value;

    StringLiteral() = default;

    explicit StringLiteral(std::string value) : value(std::move(value))
    {
    }
};

struct BooleanLiteral : public BoxableLiteral
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

struct DotLiteral : public Resolvable
{
};

struct OperatorExpression : public Resolvable
{
    int overload_index = -1;

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

struct Postfix : public OperatorExpression
{
    Expression_ptr operand;
    Token op;

    Postfix() = default;

    Postfix(Expression_ptr operand, Token op)
        : operand(std::move(operand)), op(std::move(op))
    {
    }
};

struct SequenceLiteral : public BoxableLiteral
{
    ExpressionVector expressions;

    explicit SequenceLiteral() = default;
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

struct MapLiteral : public BoxableLiteral
{
    std::map<Expression_ptr, Expression_ptr> pairs;

    explicit MapLiteral() = default;
    explicit MapLiteral(std::map<Expression_ptr, Expression_ptr> pairs)
        : pairs(std::move(pairs)) {};
};

struct Assignment : public Resolvable
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
    Expression_ptr alternative; // IfTernaryBranch or ElseTernaryBranch

    IfTernaryBranch() = default;

    IfTernaryBranch(
        Expression_ptr test,
        Expression_ptr true_expression,
        Expression_ptr alternative
    )
        : true_expression(true_expression), test(test),
          alternative(alternative) {};
};

struct ElseTernaryBranch : public TernaryBranch
{
    Expression_ptr expression;

    ElseTernaryBranch() = default;

    ElseTernaryBranch(Expression_ptr expression) : expression(expression) {};
};

// Identifiers & Access

struct Identifier : public Resolvable
{
    std::string name;

    Identifier() = default;

    Identifier(std::string name) : name(std::move(name))
    {
    }
};

// a.b
// a.b.c().d
struct MemberAccess
{
    Expression_ptr left;
    Expression_ptr right;

    int member_index = -1;
    bool is_enum_value = false;

    MemberAccess() = default;

    MemberAccess(Expression_ptr left, Expression_ptr right)
        : left(std::move(left)), right(std::move(right))
    {
    }
};

/*
foo()
A()
a.foo()
a.foo()
*/

// process()
// a.process()
// a.b.process()
// create_function()()
// a.b().c()
struct Call
{
    Expression_ptr callable;
    ExpressionVector arguments;

    bool is_method_call = false;
    bool is_pure_method_call = false;

    int overload_index;

    Call() = default;

    Call(Expression_ptr callable, ExpressionVector arguments)
        : callable(callable), arguments(std::move(arguments)),
          overload_index(-1)
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

struct Symbol;

// Foo<int>
struct TemplateAngular : public Resolvable
{
    Expression_ptr target;
    TypeAnnotationVector angular_nodes;

    std::shared_ptr<Symbol> group_symbol = nullptr;
    int overload_index = -1;

    TemplateAngular(Expression_ptr target, TypeAnnotationVector angular_nodes)
        : target(std::move(target)), angular_nodes(std::move(angular_nodes))
    {
    }
};

// Others

struct RangeLiteral
{
    Expression_ptr start;
    Expression_ptr end;
    Expression_ptr step;
    bool is_inclusive;

    RangeLiteral()
        : start(nullptr), end(nullptr), step(nullptr), is_inclusive(false)
    {
    }

    RangeLiteral(
        Expression_ptr start,
        Expression_ptr end,
        Expression_ptr step,
        bool is_inclusive
    )
        : start(std::move(start)), end(std::move(end)), step(std::move(step)),
          is_inclusive(is_inclusive)
    {
    }
};

struct InterpolatedString
{
    ExpressionVector parts;
};

// Expression

using ExpressionVariant = std::variant<
    std::monostate,

    IntegerLiteral,
    FloatLiteral,
    StringLiteral,
    BooleanLiteral,

    NoneLiteral,
    DotLiteral,
    Identifier,
    MemberAccess,

    Call,
    Constructor,
    TemplateAngular,

    Prefix,
    Infix,
    Postfix,

    ListLiteral,
    TupleLiteral,
    MapLiteral,
    SetLiteral,
    RangeLiteral,

    Assignment,

    IfTernaryBranch,
    ElseTernaryBranch>;

struct Expression : public AstNode<ExpressionVariant>
{
    using AstNode::AstNode;

    Token start_token;
    Token end_token;
};

template <typename T> inline Expression_ptr make_expression(T&& data)
{
    return std::make_shared<Expression>(std::forward<T>(data));
}

template <typename T>
inline Expression_ptr make_expression(
    T&& data,
    Token start_token,
    Token end_token
)
{
    auto expr = std::make_shared<Expression>(std::forward<T>(data));
    expr->start_token = std::move(start_token);
    expr->end_token = std::move(end_token);
    return expr;
}

} // namespace Wasp
