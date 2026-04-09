#include "AST.h"
#include "SemanticAnalyzer.h"
#include "Statement.h"
#include "SymbolScope.h"
#include "Workspace.h"

#include <ctime>
#include <memory>
#include <string>
#include <variant>

#define MAKE_OBJECT_VARIANT(x) std::make_shared<Object>(x)

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

void SemanticAnalyzer::hoist_statements(StatementVector& statements)
{
    for (auto& stmt_ptr : statements)
    {
        std::visit(
            overloaded{
                [&](FunctionDefinition& func_def)
                {
                    if (func_def.symbol)
                    {
                        return;
                    }

                    auto symbol = SymbolFactory::create_function(
                        func_def.name,
                        nullptr,
                        false,
                        nullptr,
                        nullptr,
                        current_scope->get_closure_depth(),
                        current_scope->get_lexical_depth()
                    );

                    current_scope->define(symbol);
                    func_def.symbol = symbol;
                    func_def.group_symbol = current_scope->lookup(func_def.name);
                },
                [&](ClassDefinition& class_def)
                {
                    if (class_def.symbol)
                    {
                        return;
                    }

                    auto symbol = SymbolFactory::create_class(
                        class_def.name,
                        nullptr,
                        current_scope->get_closure_depth(),
                        current_scope->get_lexical_depth()
                    );
                    current_scope->define(symbol);
                    class_def.symbol = symbol;
                },
                // Ignore other statements
                [](auto&)
                {
                }
            },
            stmt_ptr->data
        );
    }
}

// ============================================================================
// SCOPE MANAGEMENT
// ============================================================================

void SemanticAnalyzer::enter_scope(ScopeType scope_type)
{
    current_scope = std::make_shared<SymbolScope>(scope_type, current_scope);
}

void SemanticAnalyzer::leave_scope()
{
    if (current_scope)
    {
        current_scope = current_scope->get_enclosing();
    }
}

void SemanticAnalyzer::leave_scope_keep_symbol(Symbol_ptr symbol_to_keep)
{
    if (current_scope)
    {
        current_scope = current_scope->get_enclosing();

        if (current_scope)
        {
            current_scope->define(symbol_to_keep);
        }
    }
}
} // namespace Wasp
