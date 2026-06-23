#include "AST.h"
#include "Statement.h"
#include "SymbolScope.h"
#include "Terminator.h"

#include <variant>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

void Terminator::visit(Import&)
{
    // TODO: Implement
}

void Terminator::visit(Branch& stmt)
{
    enter_scope(ScopeType::BRANCH);
    visit(stmt.test);
    visit(stmt.block);

    if (stmt.alternative != nullptr)
    {
        visit(stmt.alternative);
    }

    leave_scope();
}

void Terminator::visit(SimpleLoop& stmt)
{
    enter_scope(ScopeType::LOOP);
    visit(stmt.test);
    visit(stmt.block);
    leave_scope();
}

void Terminator::visit(ForInLoop& stmt)
{
    enter_scope(ScopeType::LOOP);
    visit(stmt.lhs);
    visit(stmt.block);
    leave_scope();
}

void Terminator::visit(Return& stmt)
{
    if (stmt.expression.has_value())
    {
        visit(stmt.expression.value());
    }
}

void Terminator::visit(ExpressionStatement& stmt)
{
    visit(stmt.expression);
}

// ============================================================================
// Statements
// ============================================================================

void Terminator::visit(Block& block)
{
    for (auto& statement : block.statements)
    {
        visit(statement);
    }
}

void Terminator::visit(Statement_ptr statement)
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

} // namespace Wasp
