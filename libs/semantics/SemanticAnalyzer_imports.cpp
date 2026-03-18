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

    Symbol_ptr module_symbol = SymbolFactory::create_module(module_name, module_type, mod->exports);

    current_scope->define(module_symbol);

    import_stmt.symbol = module_symbol;
}

void SemanticAnalyzer::visit(FromImport& import_stmt) {
    auto mod = workspace->get_module(import_stmt.resolved_path);
    Doctor::get().assert(mod != nullptr, WaspStage::Semantics, "Failed to load module");

    for (auto& imported_sym : import_stmt.symbols) {

        auto it = mod->exports.find(imported_sym.name);
        Doctor::get().assert(
            it != mod->exports.end(),
            WaspStage::Semantics,
            "Module does not export symbol: " + imported_sym.name
        );

        auto original_exported_symbol = it->second;

        std::string local_name = imported_sym.alias.value_or(imported_sym.name);

        Symbol_ptr local_symbol = SymbolFactory::create_alias(local_name, original_exported_symbol);
        current_scope->define(local_symbol);

        imported_sym.symbol = local_symbol;
    }
}

} // namespace Wasp
