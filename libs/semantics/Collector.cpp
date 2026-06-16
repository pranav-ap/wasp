#include "Collector.h"
#include "AST.h"
#include "Statement.h"
#include "SymbolScope.h"
#include "Workspace.h"

#include <memory>
#include <variant>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};

template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

void Collector::run(Module_ptr mod)
{
    current_module = mod;

    enter_scope(ScopeType::MODULE);
    visit(current_module->block);
    leave_scope();
}

void Collector::enter_scope(ScopeType scope_type)
{
    auto new_scope = std::make_shared<SymbolScope>(
        scope_type,
        current_scope
    );

    current_scope = new_scope;
}

void Collector::leave_scope()
{
    if (current_scope != nullptr)
    {
        current_scope = current_scope->enclosing_scope;
    }
}

// ============================================================================
// Statements
// ============================================================================

void Collector::visit(Block& block)
{
    for (auto& statement : block.statements)
    {
        visit(statement);
    }
}

void Collector::visit(Statement_ptr statement)
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

void Collector::visit(Import&)
{
    // TODO: Implement
}

} // namespace Wasp
