#include "AST.h"
#include "Doctor.h"
#include "SemanticsAnalyzer.h"
#include "Statement.h"
#include "Symbol.h"
#include "SymbolFactory.h"
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
        current_module->get_name(),
        current_module->absolute_filepath
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

void SemanticsAnalyzer::import_symbols(Import& imp)
{
    Module_ptr imported_module = workspace->get_module(imp.module_path);

    Symbol_ptr imported_module_symbol = workspace->get_module_symbol(
        imp.module_path
    );

    if (imp.module_alias.has_value())
    {
        Symbol_ptr alias_symbol = SymbolFactory::create_symbol_alias(
            imp.module_alias.value(),
            imported_module_symbol
        );

        imp.module_symbol = alias_symbol;
        current_scope->define(alias_symbol);
    }
    else
    {
        imp.module_symbol = imported_module_symbol;

        if (!imp.expose_all)
        {
            current_scope->define(imported_module_symbol);
        }
    }

    if (imp.expose_all)
    {
        for (Symbol_ptr& exported_symbol : imported_module->exported_symbols)
        {
            ImportAsPair pair = {exported_symbol->name};
            imp.exposed_names.push_back(pair);
        }
    }

    imp.expose_all = false;

    for (ImportAsPair& pair : imp.exposed_names)
    {
        int symbol_index = imported_module->type->get_member_index(pair.name);
        Symbol_ptr exported_symbol = imported_module->exported_symbols[symbol_index];

        Doctor::semantics().fatal_if_nullptr(
            exported_symbol,
            "Module " + imported_module->get_name() + " does not export " + pair.name
        );

        if (pair.alias.has_value())
        {
            Symbol_ptr alias_symbol = SymbolFactory::create_symbol_alias(
                pair.alias.value(),
                exported_symbol
            );

            alias_symbol->module_path = imported_module->get_path();
            current_scope->define(alias_symbol);
        }
        else
        {
            current_scope->define(exported_symbol);
        }
    }
}

} // namespace Wasp
