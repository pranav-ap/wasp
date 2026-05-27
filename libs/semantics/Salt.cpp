#include "Salt.h"
#include "AST.h"
#include "ASTFactory.h"
#include "Doctor.h"
#include "Expression.h"
#include "Statement.h"
#include "Token.h"

#include <ctime>
#include <string>
#include <type_traits>
#include <variant>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

StatementVector Salt::visit(const StatementVector statements)
{
    StatementVector updated_statements;

    for (const auto& stmt : statements)
    {
        auto salty = run(stmt);
        updated_statements.push_back(salty);
    }

    return updated_statements;
}

Statement_ptr Salt::run(const Statement_ptr statement)
{
    Doctor::get().fatal_if_nullptr(statement, WaspStage::Semantics);

    return std::visit(
        [this](auto& node) -> Statement_ptr
        {
            using T = std::decay_t<decltype(node)>;
            if constexpr (requires { this->run(node); })
            {
                return this->run(node);
            }
            else
            {
                Doctor::get().fatal(
                    WaspStage::Semantics,
                    "Salt does not handle this type of statement"
                );
            }
        },
        statement->data
    );
}

Statement_ptr Salt::visit(ExpressionStatement& statement)
{
    return make_statement(visit(statement.expression));
}

Expression_ptr Salt::visit(const Expression_ptr expr)
{
    Doctor::get().fatal_if_nullptr(expr, WaspStage::Semantics);

    return std::visit(
        [&](auto& node) -> Expression_ptr
        {
            if constexpr (requires { this->run(node); })
            {
                return this->run(node);
            }
            else
            {
                Doctor::get().fatal(
                    WaspStage::Semantics,
                    "Unhandled Expression"
                );
            }
        },
        expr->data
    );
}

ExpressionVector Salt::visit(ExpressionVector expressions)
{
    ExpressionVector computed_types;
    computed_types.reserve(expressions.size());

    for (const auto& expr : expressions)
    {
        computed_types.push_back(visit(expr));
    }

    return computed_types;
}

Expression_ptr Salt::visit(Prefix& expr)
{
    std::string function_name = get_operator_name(
        TokenType::PREFIX,
        expr.op.type
    );

    ExpressionVector arguments = ExpressionVector{expr.operand};

    auto sugar = ASTFactory::create_function_call(function_name, arguments);
    return sugar;
}

Expression_ptr Salt::visit(Infix& expr)
{
    std::string function_name = get_operator_name(
        TokenType::INFIX,
        expr.op.type
    );

    ExpressionVector arguments = {expr.left, expr.right};

    auto sugar = ASTFactory::create_function_call(function_name, arguments);
    return sugar;
}

Expression_ptr Salt::visit(Postfix& expr)
{
    std::string function_name = get_operator_name(
        TokenType::POSTFIX,
        expr.op.type
    );

    ExpressionVector arguments = {expr.operand};

    auto sugar = ASTFactory::create_function_call(function_name, arguments);
    return sugar;
}

Expression_ptr visit(InterpolatedString& expr)
{
    if (expr.parts.empty())
    {
        return make_expression(StringLiteral{""});
    }

    Expression_ptr root = expr.parts[0];

    for (size_t i = 1; i < expr.parts.size(); ++i)
    {
        Token plus_token{TokenType::PLUS, "+"};
        root = make_expression(Infix{root, plus_token, expr.parts[i]});
    }

    return root;
}

} // namespace Wasp
