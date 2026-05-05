#include "SemanticAnalyzer.h"
#include "AST.h"
#include "Doctor.h"
#include "Objects.h"
#include "SymbolScope.h"
#include "Workspace.h"

#include <algorithm>
#include <ctime>
#include <memory>
#include <string>
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

StringVector SemanticAnalyzer::setup_ordered_export_names(Module_ptr mod)
{
    StringVector ordered_export_names;

    auto try_add_export = [&](const std::string& name)
    {
        if (std::find(ordered_export_names.begin(), ordered_export_names.end(), name) ==
            ordered_export_names.end())
        {
            ordered_export_names.push_back(name);
        }
    };

    auto add_if_named = [&](auto& def)
    {
        if constexpr (requires { def.name; })
        {
            try_add_export(def.name);
        }
    };

    for (auto& stmt_ptr : mod->stmts)
    {
        std::visit(
            overloaded{[&](auto& def)
                       {
                           add_if_named(def);
                       }},
            stmt_ptr->data
        );
    }

    return ordered_export_names;
}

void SemanticAnalyzer::setup_exports(Module_ptr mod, StringVector ordered_export_names)
{
    SymbolVector result;
    result.reserve(ordered_export_names.size());

    for (const auto& name : ordered_export_names)
    {
        auto symbol = current_scope->lookup(name);

        if (symbol && symbol->is_exportable())
        {
            Doctor::get().fatal_if_nullptr(
                symbol->get_type(),
                WaspStage::Semantics,
                "Symbol '" + name + "' has no type information"
            );

            mod->exports.push_back(symbol);
        }
    }
}

void SemanticAnalyzer::extract_module_type(Module_ptr module)
{
    ObjectStringMap members;
    StringVector ordered_keys;

    for (const auto& symbol : module->exports)
    {
        Doctor::get().fatal_if_nullptr(
            symbol->get_type(),
            WaspStage::Semantics,
            "Symbol '" + symbol->name + "' has no type information"
        );

        members[symbol->name] = symbol->get_type();
        ordered_keys.push_back(symbol->name);
    }

    auto module_type = std::make_shared<ModuleType>(
        module->get_name(),
        module->absolute_filepath,
        ordered_keys,
        members
    );

    module->type = make_object(module_type);
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
            [&](auto& stat)
            {
                this->visit(stat);
            }
        },
        statement->data
    );
}

} // namespace Wasp
