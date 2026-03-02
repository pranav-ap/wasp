#pragma once

#include "Token.h"
#include "TypeAnnotation.h"

#include <string>
#include <memory>
#include <variant>
#include <vector>
#include <map>
#include <stack>

namespace Wasp
{

    struct Expression;
    using Expression_ptr = std::shared_ptr<Expression>;
    using ExpressionVector = std::vector<Expression_ptr>;
    using ExpressionStack = std::stack<Expression_ptr>;

    struct Identifier
    {
        std::string name;
        Identifier() = default;
        Identifier(std::string name) : name(std::move(name)) {}
    };

    struct DotLiteral
    {
    };

    struct Prefix
    {
        Token op;
        Expression_ptr operand;
        Prefix() = default;
        Prefix(Token op, Expression_ptr operand)
            : op(std::move(op)), operand(std::move(operand)) {}
    };

    struct Infix
    {
        Expression_ptr left;
        Token op;
        Expression_ptr right;

        Infix() = default;

        Infix(Expression_ptr left, Token op, Expression_ptr right)
            : left(std::move(left)), op(std::move(op)), right(std::move(right)) {}
    };

    struct Postfix
    {
        Expression_ptr operand;
        Token op;

        Postfix() = default;
        Postfix(Expression_ptr operand, Token op)
            : operand(std::move(operand)), op(std::move(op)) {}
    };

    struct SequenceLiteral
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

    struct MapLiteral
    {
        std::map<Expression_ptr, Expression_ptr> pairs;

        explicit MapLiteral() = default;
        explicit MapLiteral(std::map<Expression_ptr, Expression_ptr> pairs)
            : pairs(std::move(pairs)) {};
    };

    struct TypePattern
    {
        Expression_ptr expression;
        TypeAnnotation_ptr type_node;

        TypePattern() = default;
        TypePattern(Expression_ptr expression, TypeAnnotation_ptr type_node)
            : expression(std::move(expression)), type_node(std::move(type_node)) {};
    };

    struct VariableDefinitionExpression
    {
        Expression_ptr assignment;
        bool is_mutable;

        VariableDefinitionExpression() = default;
        VariableDefinitionExpression(Expression_ptr assignment, bool is_mutable = false)
            : assignment(assignment), is_mutable(is_mutable) {};
    };

    struct Assignment
    {
        Expression_ptr lhs_expression;
        Expression_ptr rhs_expression;

        Assignment() = default;
        Assignment(Expression_ptr lhs_expression, Expression_ptr rhs_expression)
            : lhs_expression(std::move(lhs_expression)), rhs_expression(std::move(rhs_expression)) {}
    };

    struct UntypedAssignment : public Assignment
    {
        using Assignment::Assignment;
    };

    struct TypedAssignment : public Assignment
    {
        TypeAnnotation_ptr type_node;

        TypedAssignment() : Assignment(), type_node(nullptr) {}

        TypedAssignment(Expression_ptr lhs_expression, Expression_ptr rhs_expression, TypeAnnotation_ptr type_node)
            : Assignment(std::move(lhs_expression), std::move(rhs_expression)),
              type_node(std::move(type_node)) {}
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

        IfTernaryBranch(Expression_ptr test, Expression_ptr true_expression, Expression_ptr alternative)
            : true_expression(true_expression), test(test), alternative(alternative) {};
    };

    struct ElseTernaryBranch : public TernaryBranch
    {
        Expression_ptr expression;

        ElseTernaryBranch() = default;

        ElseTernaryBranch(Expression_ptr expression)
            : expression(expression) {};
    };

    // Other

    struct Call
    {
        Expression_ptr callee;
        ExpressionVector arguments;

        Call() = default;

        Call(Expression_ptr callee, ExpressionVector arguments)
            : callee(std::move(callee)), arguments(std::move(arguments)) {};
    };

    struct RangeLiteral
    {
        Expression_ptr start; // nullptr for ..10
        Expression_ptr end;   // nullptr for 1..
        Expression_ptr step;  // nullptr for 1..10
        bool is_inclusive;    // true for ..., false for ..

        RangeLiteral() : start(nullptr), end(nullptr), step(nullptr), is_inclusive(false) {}

        RangeLiteral(Expression_ptr start, Expression_ptr end, Expression_ptr step, bool is_inclusive)
            : start(std::move(start)), end(std::move(end)), step(std::move(step)), is_inclusive(is_inclusive) {}
    };

    // Expression Variant

    struct Expression
    {
        std::variant<
            std::monostate,

            int, double, std::string, bool,

            Identifier, DotLiteral,

            Prefix, Infix, Postfix,

            ListLiteral, TupleLiteral, MapLiteral, SetLiteral, RangeLiteral,

            VariableDefinitionExpression,
            UntypedAssignment, TypedAssignment,
            TypePattern,

            IfTernaryBranch, ElseTernaryBranch,

            Call>
            data;

        Expression() = default;

        template <typename T>
        Expression(T &&val) : data(std::forward<T>(val)) {}

        template <typename T>
        [[nodiscard]] bool is() const
        {
            return std::holds_alternative<T>(data);
        }

        template <typename T>
        const T &as() const
        {
            return std::get<T>(data);
        }

        template <typename T>
        const T *try_as() const
        {
            return std::get_if<T>(&data);
        }
    };
}
