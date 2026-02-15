#pragma once

#include "Token.h"
#include "TypeAnnotation.h"

#include <string>
#include <memory>
#include <variant>
#include <vector>
#include <map>
#include <stack>


namespace Wasp {
    struct Expression;
    using Expression_ptr = std::shared_ptr<Expression>;
    using ExpressionVector = std::vector<Expression_ptr>;
    using ExpressionStack = std::stack<Expression_ptr>;

    struct Identifier {
        std::string name;
        Identifier(std::string name) : name(std::move(name)) {}
    };

    struct Prefix {
        Token op;
        Expression_ptr operand;
        Prefix(Token op, Expression_ptr operand) 
            : op(std::move(op)), operand(std::move(operand)) {}
    };

    struct Infix {
        Expression_ptr left;
        Token op;
        Expression_ptr right;
        Infix(Expression_ptr left, Token op, Expression_ptr right)
            : left(std::move(left)), op(std::move(op)), right(std::move(right)) {}
    };

    struct Postfix {
        Expression_ptr operand;
        Token op;
        Postfix(Expression_ptr operand, Token op) 
            : operand(std::move(operand)), op(std::move(op)) {}
    };

    struct SequenceLiteral {
        ExpressionVector expressions;
        SequenceLiteral(ExpressionVector expressions)
            : expressions(std::move(expressions)) {};
    };

    struct ListLiteral : public SequenceLiteral {
        ListLiteral(ExpressionVector expressions)
            : SequenceLiteral(std::move(expressions)) {};
    };

    struct TupleLiteral : public SequenceLiteral {
        TupleLiteral(ExpressionVector expressions)
            : SequenceLiteral(std::move(expressions)) {};
    };

    struct SetLiteral : public SequenceLiteral {
        SetLiteral(ExpressionVector expressions)
            : SequenceLiteral(std::move(expressions)) {};
    };

    struct MapLiteral {
        std::map<Expression_ptr, Expression_ptr> pairs;

        MapLiteral(std::map<Expression_ptr, Expression_ptr> pairs)
            : pairs(std::move(pairs)) {};
    };

    struct TypePattern {
        Expression_ptr expression;
        TypeAnnotation_ptr type_node;

        TypePattern(Expression_ptr expression, TypeAnnotation_ptr type_node)
            : expression(std::move(expression)), type_node(std::move(type_node)) {};
    };

    struct VariableDefinitionExpression {
        Expression_ptr assignment;
        bool is_mutable;

        VariableDefinitionExpression(Expression_ptr assignment, bool is_mutable = false)
            : assignment(assignment), is_mutable(is_mutable) {};
    };

    struct Assignment
    {
        Expression_ptr lhs_expression;
        Expression_ptr rhs_expression;

        Assignment(Expression_ptr lhs_expression, Expression_ptr rhs_expression)
            : lhs_expression(lhs_expression), rhs_expression(rhs_expression) {};
    };

    struct UntypedAssignment : public Assignment
    {
        UntypedAssignment(Expression_ptr lhs_expression, Expression_ptr rhs_expression)
            : Assignment(lhs_expression, rhs_expression) {};
    };

    struct TypedAssignment : public Assignment
    {
        TypeAnnotation_ptr type_node;

        TypedAssignment(Expression_ptr lhs_expression, Expression_ptr rhs_expression, TypeAnnotation_ptr type_node)
            : Assignment(lhs_expression, rhs_expression), type_node(std::move(type_node)) {};
    };

    // Branching

    struct TernaryBranch {};

    struct IfTernaryBranch : public TernaryBranch {
        Expression_ptr test;
        Expression_ptr true_expression;
        Expression_ptr alternative; // IfTernaryBranch or ElseTernaryBranch

        IfTernaryBranch(Expression_ptr test, Expression_ptr true_expression, Expression_ptr alternative)
            : true_expression(true_expression), test(test), alternative(alternative) {};
    };

    struct ElseTernaryBranch : public TernaryBranch {
        Expression_ptr expression;

        ElseTernaryBranch(Expression_ptr expression) 
            : expression(expression) {};
    };

    // Other

    struct Call {
        Expression_ptr callee;
        ExpressionVector arguments;

        Call(Expression_ptr callee, ExpressionVector arguments)
            : callee(std::move(callee)), arguments(std::move(arguments)) {};
    };

    struct RangeLiteral {
        Expression_ptr start; // nullptr for ..10
        Expression_ptr end;   // nullptr for 1..
        Expression_ptr step;  // nullptr for 1..10
        bool is_inclusive;    // true for ..., false for ..

        RangeLiteral(Expression_ptr start, Expression_ptr end, Expression_ptr step, bool is_inclusive)
            : start(std::move(start)), end(std::move(end)), step(std::move(step)), is_inclusive(is_inclusive) {}
    };

    // Expression Variant

    struct Expression {
        std::variant<
            std::monostate,
            int, double, std::string, bool,
            Identifier,
            Prefix, Infix, Postfix,
            ListLiteral, TupleLiteral, SetLiteral, MapLiteral,
            RangeLiteral,
            
            TypePattern,
            VariableDefinitionExpression,
	        UntypedAssignment, TypedAssignment,
            
	        IfTernaryBranch, ElseTernaryBranch,

            Call
        > data;

        Expression() = default;
    
        // Generic constructor for any variant type
        template<typename T>
        Expression(T&& val) : data(std::forward<T>(val)) {}

        template<typename T>
        [[nodiscard]] bool is() const { return std::holds_alternative<T>(data); }

        template<typename T>
        const T &as() const { return std::get<T>(data); }
    };
}
