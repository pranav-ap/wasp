#include "SemanticAnalyzer.h"
#include "AST.h"
#include "Doctor.h"
#include "Objects.h"
#include "Statement.h"
#include "SymbolScope.h"
#include "Workspace.h"

#include <ctime>
#include <memory>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

void SemanticAnalyzer::run(const std::vector<Module_ptr>& build_order)
{
    enter_scope(ScopeType::WORKSPACE);

    for (const auto mod : build_order)
    {
        current_module = mod;

        enter_scope(ScopeType::MODULE);
        hoist_statements(mod->stmts);

        for (const auto& stmt : mod->stmts)
        {
            visit(stmt);
        }

        StringVector ordered_export_names = setup_ordered_export_names(mod);
        setup_exports(mod, ordered_export_names);
        leave_scope();
        extract_module_type(mod);
    }

    leave_scope();
}

void SemanticAnalyzer::visit(std::vector<Statement_ptr>& statements)
{
    hoist_statements(statements);

    for (const auto& stmt : statements)
    {
        visit(stmt);
    }
}

void SemanticAnalyzer::visit(const Statement_ptr statement)
{
    Doctor::get().fatal_if_nullptr(statement, WaspStage::Semantics);

    std::visit(
        overloaded{
            [&](std::monostate&)
            {
                Doctor::get().fatal(
                    WaspStage::Semantics,
                    "Unhandled Statement in Semantic Analyzer!"
                );
            },
            [&](FieldDefinition&)
            {
                Doctor::get().fatal(
                    WaspStage::Semantics,
                    "Field definitions are not valid statements on their own."
                );
            },
            [&](auto& stat)
            {
                using T = std::decay_t<decltype(stat)>;

                if constexpr (
                    std::is_same_v<T, Import> ||
                    std::is_same_v<T, MethodDefinition>
                )
                {
                    return;
                }
                else
                {
                    this->visit(stat);
                }
            }
        },
        statement->data
    );
}

} // namespace Wasp
