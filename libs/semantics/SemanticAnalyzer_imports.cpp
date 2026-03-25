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

    Symbol_ptr module_symbol = SymbolFactory::create_module(mod->get_name(), mod);

    if (import_stmt.alias.has_value())
    {
        Symbol_ptr alias_symbol = SymbolFactory::create_alias(
            import_stmt.alias.value(),
            module_symbol);

        current_scope->define(alias_symbol);
        import_stmt.symbol = module_symbol;

        return;
    }

    current_scope->define(module_symbol);
    import_stmt.symbol = module_symbol;
}

void SemanticAnalyzer::visit(FromImport& import_stmt)
{
    auto mod = workspace->get_module(import_stmt.absolute_path);
    Doctor::get().fatal_if_nullptr(mod, WaspStage::Semantics, "Failed to load module");

    Symbol_ptr module_symbol = SymbolFactory::create_module(mod->get_name(), mod);
    import_stmt.symbol = module_symbol;

    SymbolVector exports = mod->get_flat_exports();

    for (auto& imported_symbol_node : import_stmt.symbols)
    {
        std::string target_name = imported_symbol_node.name;
        std::string local_name = imported_symbol_node.alias.value_or(target_name);

        SymbolVector symbols_with_target_name;

        for (const auto& exported_symbol : exports)
        {
            if (exported_symbol->name == target_name)
            {
                symbols_with_target_name.push_back(exported_symbol);
            }
        }

        Doctor::get().assert(
            !symbols_with_target_name.empty(),
            WaspStage::Semantics,
            "Module '" + mod->get_name() + "' does not export symbol: " + target_name);

        for (const auto& exported_symbol : symbols_with_target_name)
        {
            Symbol_ptr local_symbol = SymbolFactory::create_alias(local_name, exported_symbol);
            current_scope->define(local_symbol);

            imported_symbol_node.resolved_symbols.push_back(local_symbol);
        }
    }
}

} // namespace Wasp
