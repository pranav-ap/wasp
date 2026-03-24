#include "SemanticAnalyzer.h"
#include "AST.h"
#include "Doctor.h"
#include "Objects.h"
#include "Statement.h"

#include "SymbolScope.h"
#include "Workspace.h"

#include <algorithm>
#include <ctime>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#define MAKE_OBJECT_VARIANT(x) std::make_shared<Object>(x)

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{
// ============================================================================
// ENTRY POINT
// ============================================================================

void SemanticAnalyzer::extract_module_type(Module_ptr module)
{
    auto exports = module->get_flat_exports();

    // Sort the exports by Symbol ID.
    // This perfectly mirrors how the VM will pack its physical array!
    std::sort(
        exports.begin(),
        exports.end(),
        [](const Symbol_ptr& a, const Symbol_ptr& b) { return a->id < b->id; });

    // Validate that everything has a type
    for (const auto& symbol : exports)
    {
        Doctor::get().fatal_if_nullptr(
            symbol->get_type(),
            WaspStage::Semantics,
            "Symbol '" + symbol->name + "' has no type information");
    }

    auto module_type = ModuleType(
        module->get_name(),
        module->absolute_filepath,
        std::move(exports));

    module->type = make_object(module_type);
}

void SemanticAnalyzer::run(const std::vector<Module_ptr>& build_order)
{
    enter_scope(ScopeType::WORKSPACE);
    register_natives();

    for (const auto& module : build_order)
    {
        enter_scope(ScopeType::MODULE);

        // Push this module's hoisted symbols into its scope before visiting the statements,
        // so that they can be referenced in the module body
        for (auto& symbol : module->get_flat_hoists())
        {
            current_scope->define(symbol);
        }

        visit(module->stmts);
        leave_scope();

        extract_module_type(module);
    }

    leave_scope();
}

void SemanticAnalyzer::register_natives()
{
    std::unordered_map<std::string, int> native_names = workspace->native_registry
                                                            ->get_all_native_names();

    for (const auto& [name, index] : native_names)
    {
        auto symbol_type = workspace->native_registry->get_native_object_type(index);

        auto symbol = SymbolFactory::create_function(name, symbol_type, true);
        current_scope->define(symbol);
    }
}

// ============================================================================
// High Level Visitors
// ============================================================================

void SemanticAnalyzer::visit(std::vector<Statement_ptr>& statements)
{
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
            [](auto)
            {
                Doctor::get().Doctor::get().fatal(
                    WaspStage::Semantics,
                    "Unhandled Statement in Semantic Analyzer!");
            }},
        statement->data);
}
} // namespace Wasp
