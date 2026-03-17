#include "SemanticAnalyzer.h"
#include "Doctor.h"
#include "Objects.h"
#include "Statement.h"
#include "Symbol.h"
#include "SymbolScope.h"
#include "Workspace.h"

#include <ctime>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#define MAKE_OBJECT_VARIANT(x) std::make_shared<Object>(x)

template <class... Ts> struct overloaded : Ts... {
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp {
// ============================================================================
// ENTRY POINT
// ============================================================================

void SemanticAnalyzer::extract_module_type(Module_ptr module) {
    std::map<std::string, Object_ptr> module_members;

    for (const auto& [name, symbol] : module->exports) {
        Object_ptr resolved_type = symbol->get_type();

        Doctor::get().fatal_if_nullptr(
            resolved_type,
            WaspStage::Semantics,
            "Compiler Error: Exported symbol '" + name + "' failed to resolve a type."
        );

        module_members[name] = resolved_type;
    }

    module->type = std::make_shared<Object>(ModuleType(std::move(module_members)));
}

void SemanticAnalyzer::run(const std::vector<Module_ptr>& build_order) {
    enter_scope(ScopeType::WORKSPACE);
    register_natives();

    for (const auto& module : build_order) {
        enter_scope(ScopeType::MODULE);

        // Push the hoisted exports into this module's scope
        for (const auto& [name, symbol] : module->exports) {
            current_scope->define(symbol);
        }

        visit(module->block);
        leave_scope();

        extract_module_type(module);
    }

    leave_scope();
}

void SemanticAnalyzer::register_natives() {
    std::unordered_map<std::string, int> native_names = native_registry->get_all_native_names();

    for (const auto& [name, index] : native_names) {
        auto symbol_type = native_registry->get_native_object_type(index);

        auto symbol = SymbolFactory::create_function(name, symbol_type, true);

        current_scope->define(symbol);
    }
}

// ============================================================================
// High Level Visitors
// ============================================================================

void SemanticAnalyzer::visit(std::vector<Statement_ptr>& statements) {
    for (const auto& stmt : statements) {
        visit(stmt);
    }
}

void SemanticAnalyzer::visit(const Statement_ptr statement) {
    Doctor::get().fatal_if_nullptr(statement, WaspStage::Semantics);

    std::visit(
        overloaded{
            [&](ExpressionStatement& stat) { visit(stat); },
            [&](VariableDefinition& stat) { visit(stat); },
            [&](AliasDefinition& stat) { visit(stat); },
            [&](EnumDefinition& stat) { visit(stat); },
            [&](FunctionDefinition& stat) { visit(stat); },
            [&](ClassDefinition& stat) { visit(stat); },
            [&](TraitDefinition& stat) { visit(stat); },
            [&](ImplDefinition& stat) { visit(stat); },
            [&](AnnotationDefinition& stat) { visit(stat); },
            [&](IfBranch& stat) { visit(stat); },
            [&](ElseBranch& stat) { visit(stat); },
            [&](SimpleLoop& stat) { visit(stat); },
            [&](ForInLoop& stat) { visit(stat); },
            [&](LoopControl& stat) { visit(stat); },
            [&](Pass& stat) { visit(stat); },
            [&](Return& stat) { visit(stat); },
            [&](SimpleImport& stat) { visit(stat); },
            [&](FromImport& stat) { visit(stat); },
            [](auto) {
                Doctor::get().Doctor::get().fatal(
                    WaspStage::Semantics, "Unhandled Statement in Semantic Analyzer!"
                );
            }
        },
        statement->data
    );
}
} // namespace Wasp
