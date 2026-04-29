#include "Doctor.h"
#include "SemanticAnalyzer.h"
#include "Statement.h"
#include "Workspace.h"

#include <ctime>
#include <memory>
#include <string>

#define MAKE_OBJECT_VARIANT(x) std::make_shared<Object>(x)

template <class... Ts> struct overloaded : Ts... {
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp {

void SemanticAnalyzer::visit(SimpleImport& import_stmt)
{
    auto mod = workspace->get_module(import_stmt.absolute_path);
    Doctor::get().fatal_if_nullptr(mod, WaspStage::Semantics, "Failed to load module");

    std::string module_name = mod->get_name();
    Symbol_ptr module_symbol = SymbolFactory::create_module(module_name, mod);

    std::string module_path = mod->get_path();
    for (auto& exported_symbol : mod->exports)
    {
        exported_symbol->module_path = module_path;
    }

    if (import_stmt.alias.has_value())
    {
        std::string alias_name = import_stmt.alias.value();
        Symbol_ptr alias_symbol = SymbolFactory::create_alias(alias_name, module_symbol);

        current_scope->define(alias_symbol);
        import_stmt.symbol = module_symbol;
    }
    else
    {
        current_scope->define(module_symbol);
        import_stmt.symbol = module_symbol;
    }
}

void SemanticAnalyzer::visit(FromImport& import_stmt)
{
    auto mod = workspace->get_module(import_stmt.absolute_path);
    Doctor::get().fatal_if_nullptr(mod, WaspStage::Semantics, "Failed to load module");

    Symbol_ptr module_symbol = SymbolFactory::create_module(mod->get_name(), mod);
    import_stmt.symbol = module_symbol;

    std::string module_path = mod->get_path();

    for (auto& imported_symbol_node : import_stmt.symbols)
    {
        std::string target_name = imported_symbol_node.name;
        std::string local_name = imported_symbol_node.alias.value_or(target_name);

        SymbolVector exports;

        for (const auto& exported_symbol : mod->exports)
        {
            if (exported_symbol->name == target_name)
            {
                exported_symbol->module_path = module_path;
                exports.push_back(exported_symbol);
            }
        }

        Doctor::get().assert(
            !exports.empty(),
            WaspStage::Semantics,
            "Module '" + mod->get_name() + "' does not export symbol: " + target_name
        );

        for (const auto& exported_symbol : exports)
        {
            Symbol_ptr local_symbol = SymbolFactory::create_alias(local_name, exported_symbol);
            local_symbol->module_path = module_path;

            current_scope->define(local_symbol);
            imported_symbol_node.resolved_symbols.push_back(local_symbol);
        }
    }
}

} // namespace Wasp
