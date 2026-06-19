#include "Salter.h"
#include "AST.h"
#include "Expression.h"
#include "Statement.h"
#include "Token.h"

#include <cstddef>
#include <variant>

namespace Wasp
{

// ============================================================================
// Main Entry Point
// ============================================================================

void Salter::visit(Block& block)
{
    for (auto& statement : block.statements)
    {
        visit(statement);
    }
}

void Salter::visit(Statement_ptr statement)
{
    std::visit(
        [&](auto& node)
        {
            if constexpr (requires { visit(node); })
            {
                visit(node);
            }
        },
        statement->data
    );
}

// ============================================================================
// Statement Visitors
// ============================================================================

void Salter::visit(FunctionDefinition& statement)
{
    visit(statement.block);
}

void Salter::visit(MethodDefinition& statement)
{
    visit(statement.block);
}

void Salter::visit(OperatorDefinition& statement)
{
    visit(statement.block);
}

void Salter::visit(ExpressionStatement& statement)
{
    visit(statement.expression);
}

// ============================================================================
// Expression Visitors
// ============================================================================

Expression_ptr Salter::visit(Expression_ptr expression)
{
    return std::visit(
        [&](auto& node) -> Expression_ptr
        {
            if constexpr (requires { visit(node); })
            {
                return visit(node);
            }
            else
            {
                return nullptr;
            }
        },
        expression->data
    );
}

Expression_ptr Salter::visit(InterpolatedString& binding)
{
    ExpressionVector new_parts;

    for (auto& part : binding.parts)
    {
        Expression_ptr new_part = visit(part);
        new_parts.push_back(new_part);
    }

    // add infix

    for (size_t i = 0; i < new_parts.size() - 1; i++)
    {
        auto& left = new_parts[i];
        auto& right = new_parts[i + 1];

        auto infix_expr = make_expression(
            Infix{left, Token(TokenType::PLUS), right}
        );

        new_parts[i + 1] = infix_expr;
    }

    return new_parts;
}

} // namespace Wasp
