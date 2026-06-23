#include "SemanticsAnalyzer.h"
#include "AST.h"
#include "Collector.h"
#include "Doctor.h"
#include "Hoister.h"
#include "Symbol.h"
#include "SymbolFactory.h"
#include "SymbolScope.h"
#include "Terminator.h"
#include "Type.h"
#include "Workspace.h"

#include <algorithm>
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

namespace
{

StringVector get_ordered_definition_names(Module_ptr mod)
{
    StringVector ordered_definition_names;

    for (auto& stmt_ptr : mod->block.statements)
    {
        std::visit(
            overloaded{[&](auto& def)
                       {
                           if constexpr (requires { def.name; })
                           {
                               if (std::find(
                                       ordered_definition_names.begin(),
                                       ordered_definition_names.end(),
                                       def.name
                                   ) == ordered_definition_names.end())
                               {
                                   ordered_definition_names.push_back(def.name);
                               }
                           }
                       }},
            stmt_ptr->data
        );
    }

    return ordered_definition_names;
}

} // namespace

void SemanticsAnalyzer::init_module(Module_ptr current_module)
{
    StringVector ordered_definition_names = get_ordered_definition_names(
        current_module
    );

    ModuleType_ptr mod_type = std::make_shared<ModuleType>(
        current_module->get_name()
    );

    TypeStringMap exported_types;
    StringVector ordered_exported_definition_names;
    SymbolVector exported_symbols;

    for (const std::string& def_name : ordered_definition_names)
    {
        Symbol_ptr symbol = current_scope->lookup_required(def_name);

        symbol->module_path = current_module->get_path();

        if (symbol->is_exportable())
        {
            Type_ptr export_type = symbol->get_type();

            Doctor::semantics().fatal_if_nullptr(
                export_type,
                "Symbol '" + def_name + "' has no type information"
            );

            exported_types[def_name] = export_type;
            ordered_exported_definition_names.push_back(def_name);
            exported_symbols.push_back(symbol);
        }
    }

    mod_type->member_types = exported_types;
    mod_type->ordered_keys = ordered_exported_definition_names;

    current_module->exported_symbols = exported_symbols;
    current_module->type = mod_type;

    Symbol_ptr module_symbol = SymbolFactory::create_module(
        current_module->get_name(),
        make_type(mod_type)
    );

    workspace->add_module_symbol(current_module->absolute_filepath, module_symbol);
}

void SemanticsAnalyzer::run(std::vector<Module_ptr>& build_order)
{
    Doctor::semantics().start();

    enter_scope(ScopeType::WORKSPACE);

    for (Module_ptr& current_module : build_order)
    {
        enter_scope(ScopeType::MODULE);

        Hoister hoister(workspace, current_module, current_scope);
        hoister.run();

        leave_scope();

        enter_scope(ScopeType::MODULE);

        Collector collector(workspace, current_module, current_scope);
        collector.run();

        leave_scope();

        auto ast_forest = collector.get_forest();

        enter_scope(ScopeType::MODULE);

        Terminator terminator(workspace, current_module, current_scope, ast_forest);
        terminator.run();

        current_module->save_ast("semantics");

        init_module(current_module);

        leave_scope();
    }

    leave_scope();

    Doctor::semantics().stop();
}

void SemanticsAnalyzer::enter_scope(ScopeType scope_type)
{
    auto new_scope = std::make_shared<SymbolScope>(
        scope_type,
        current_scope
    );

    current_scope = new_scope;
}

void SemanticsAnalyzer::leave_scope()
{
    if (current_scope != nullptr)
    {
        current_scope = current_scope->enclosing_scope;
    }
}

} // namespace Wasp
