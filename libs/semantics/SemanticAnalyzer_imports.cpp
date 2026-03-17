#include "SemanticAnalyzer.h"
#include "Doctor.h"
#include "Objects.h"
#include "Statement.h"
#include "Symbol.h"

#include <ctime>
#include <map>
#include <memory>
#include <string>
#include <utility>

#define MAKE_OBJECT_VARIANT(x) std::make_shared<Object>(x)

template <class... Ts> struct overloaded : Ts... {
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp {
void SemanticAnalyzer::visit(SimpleImport& import_stmt) {
    auto mod = workspace->get_module(import_stmt.resolved_path);
    Doctor::get().assert(mod != nullptr, WaspStage::Semantics, "Failed to load module");

    std::string module_name = import_stmt.alias.value_or(import_stmt.resolved_path.stem().string());

    std::map<std::string, Object_ptr> member_types;
    for (const auto& [name, exported_symbol] : mod->exports) {
        member_types[name] = exported_symbol->get_type();
    }

    auto module_type = std::make_shared<Object>(ModuleType(std::move(member_types)));

    auto module_symbol = SymbolFactory::create_module(module_name, module_type, mod->exports);
    current_scope->define(module_symbol);

    import_stmt.symbol = module_symbol;
}

void SemanticAnalyzer::visit(FromImport& import_stmt) {
    auto mod = workspace->get_module(import_stmt.resolved_path);
    Doctor::get().fatal_if_nullptr(mod, WaspStage::Semantics);

    // `from math import sqrt, pi as pie`
    for (const auto& sym : import_stmt.symbols) {
        // Check if the dependency actually exports the requested name
        auto it = mod->exports.find(sym.name);

        Doctor::get().assert(
            it != mod->exports.end(),
            WaspStage::Semantics,
            "Import Error: Module '" + import_stmt.resolved_path.stem().string() +
                "' does not export '" + sym.name + "'."
        );

        Symbol_ptr symbol_to_define = it->second;

        // If aliased, we must safely clone it using the SymbolFactory
        if (sym.alias.has_value()) {
            std::string alias_name = sym.alias.value();
            symbol_to_define = SymbolFactory::create_alias(alias_name, it->second);
        }

        current_scope->define(symbol_to_define);
    }
}

} // namespace Wasp
