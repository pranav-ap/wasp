#include "Doctor.h"
#include "SemanticAnalyzer.h"
#include "Statement.h"
#include "Workspace.h"

#include <ctime>
#include <memory>
#include <string>

template <class... Ts> struct overloaded : Ts... {
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp {

void SemanticAnalyzer::visit(SimpleImport& import_stmt)
{
    auto mod = workspace->get_module(import_stmt.absolute_path);
    Doctor::get().fatal_if_nullptr(mod, WaspStage::Semantics);

    Symbol_ptr module_symbol = SymbolFactory::create_module(mod->get_name(), mod);

    for (auto& exported_symbol : mod->exports)
    {
        exported_symbol->module_path = mod->get_path();
    }

    if (import_stmt.alias.has_value())
    {
        std::string alias_name = import_stmt.alias.value();
        Symbol_ptr alias_symbol = SymbolFactory::create_alias(alias_name, module_symbol);

        current_scope->define(alias_symbol);
        import_stmt.symbol = alias_symbol;
    }
    else
    {

        import_stmt.symbol = current_scope->define(module_symbol);
    }
}

void SemanticAnalyzer::visit(FromImport& import_stmt)
{
    auto mod = workspace->get_module(import_stmt.absolute_path);
    Doctor::get().fatal_if_nullptr(mod, WaspStage::Semantics);

    import_stmt.symbol = SymbolFactory::create_module(mod->get_name(), mod);
    std::string module_path = mod->get_path();

    for (auto& pair : import_stmt.import_as_pairs)
    {
        Symbol_ptr exported_symbol = mod->get_member(pair.name);

        Doctor::get().assert(
            exported_symbol != nullptr,
            WaspStage::Semantics,
            "Module '" + mod->get_name() + "' does not export symbol: " + pair.name
        );

        exported_symbol->module_path = module_path;

        if (pair.alias.has_value())
        {
            std::string alias_name = pair.alias.value();
            Symbol_ptr alias_symbol = SymbolFactory::create_alias(
                alias_name,
                exported_symbol
            );

            alias_symbol->module_path = module_path;

            current_scope->define(alias_symbol);
            pair.symbol = alias_symbol;
        }
        else
        {
            pair.symbol = current_scope->define(exported_symbol);
        }
    }
}

} // namespace Wasp
