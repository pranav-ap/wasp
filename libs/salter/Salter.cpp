#include "Salter.h"
#include "AST.h"
#include "Doctor.h"
#include "Expression.h"
#include "Statement.h"
#include "SymbolScope.h"
#include "Token.h"
#include "Workspace.h"

#include <cstddef>
#include <memory>
#include <variant>
#include <vector>

namespace Wasp
{

// ============================================================================
// Main Entry Point
// ============================================================================

void Salter::run(const std::vector<Module_ptr>& build_order)
{
    Doctor::get().start();

    enter_scope(ScopeType::WORKSPACE);

    for (const auto& mod : build_order)
    {
        current_module = mod;

        enter_scope(ScopeType::MODULE);
        mod->block = salt(mod->block);
        leave_scope();
    }

    leave_scope();

    Doctor::get().stop();
}

void Salter::enter_scope(ScopeType scope_type)
{
    auto new_scope = std::make_shared<SymbolScope>(scope_type, current_scope);

    current_scope = new_scope;
}

void Salter::leave_scope()
{
    if (current_scope != nullptr)
    {
        current_scope = current_scope->enclosing_scope;
    }
}

// ============================================================================
// Statement Visitors
// ============================================================================

Statement_ptr Salter::visit(Statement_ptr statement)
{
    return std::visit(
        [&](auto& node) -> Statement_ptr
        {
            if constexpr (requires { visit(node); })
            {
                return visit(node);
            }

            return statement;
        },
        statement->data
    );
}

Block Salter::salt(Block& block)
{
    Block new_block;

    for (auto& statement : block.statements)
    {
        Statement_ptr new_stmt = visit(statement);

        if (new_stmt->is<Block>())
        {
            for (auto& stmt : new_stmt->as<Block>().statements)
            {
                new_block.add(stmt);
            }

            continue;
        }

        new_block.add(new_stmt);
    }

    return new_block;
}

Statement_ptr Salter::visit(Block& block)
{
    Block new_block = salt(block);
    return make_statement(new_block);
}

Statement_ptr Salter::visit(ExpressionStatement& statement)
{
    Expression_ptr expr = visit(statement.expression);
    return make_statement(ExpressionStatement{expr});
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

            return expression;
        },
        expression->data
    );
}

Expression_ptr Salter::visit(InterpolatedString& binding)
{
    if (binding.parts.empty())
    {
        return make_expression(StringLiteral{""});
    }

    ExpressionVector new_parts;

    for (auto& part : binding.parts)
    {
        Expression_ptr new_part = visit(part);
        new_parts.push_back(new_part);
    }

    if (new_parts.size() == 1)
    {
        return new_parts.front();
    }

    Expression_ptr result = new_parts.front();

    for (size_t i = 1; i < new_parts.size(); i++)
    {
        result = make_expression(
            Infix{result, Token(TokenType::PLUS, "+"), new_parts[i]}
        );
    }

    return result;
}

} // namespace Wasp
