#include "Phase.h"
#include "AST.h"
#include "Doctor.h"
#include "Statement.h"
#include "Symbol.h"
#include "SymbolFactory.h"
#include "SymbolScope.h"
#include "Workspace.h"

#include <memory>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};

template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

void Phase::run()
{
    visit(current_module->block);
}

void Phase::enter_scope(ScopeType scope_type)
{
    auto new_scope = std::make_shared<SymbolScope>(scope_type, current_scope);
    current_scope = new_scope;
}

void Phase::leave_scope()
{
    if (current_scope != nullptr)
    {
        current_scope = current_scope->enclosing_scope;
    }
}

void Phase::import_symbols(Import& imp)
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
