#include "Collector.h"
#include "AST.h"
#include "Doctor.h"
#include "Statement.h"
#include "SymbolScope.h"

#include <tuple>
#include <variant>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};

template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

std::tuple<Statement_ptr, SymbolScope_ptr> Collector::get_tree(Symbol_ptr symbol)
{
    auto it = forest.find(symbol);

    Doctor::semantics().check(
        it != forest.end(),
        "No AST found for symbol '" + symbol->name + "'"
    );

    return {it->second, scope_forest[symbol]};
}

// ============================================================================
// Statements
// ============================================================================

void Collector::hoist(Block& block)
{
    for (auto& statement : block.statements)
    {
        hoist(statement);
    }
}

void Collector::hoist(Statement_ptr statement)
{
    std::visit(
        overloaded{
            [&](FunctionDefinition& def)
            {
                current_scope->define_function_overload(def.overload_symbol);
            },
            [&](ClassDefinition& def)
            {
                current_scope->define(def.symbol);
            },
            [&](TraitDefinition& def)
            {
                current_scope->define(def.symbol);
            },
            [&](PrimitiveDefinition& def)
            {
                current_scope->define(def.symbol);
            },
            [&](TypeAliasDefinition& def)
            {
                current_scope->define(def.symbol);
            },
            [&](EnumDefinition& def)
            {
                current_scope->define(def.symbol);
            },
            [&](auto& node)
            {
                // do nothing
            }
        },
        statement->data
    );
}

void Collector::visit(Block& block)
{
    hoist(block);

    for (auto& statement : block.statements)
    {
        visit(statement);
    }
}

void Collector::visit(Statement_ptr statement)
{
    std::visit(
        overloaded{
            [&](Import& imp)
            {
                import_symbols(imp);
            },
            [&](auto& node)
            {
                if constexpr (requires { visit(node); })
                {
                    visit(node);
                }
            }
        },
        statement->data
    );
}

} // namespace Wasp
