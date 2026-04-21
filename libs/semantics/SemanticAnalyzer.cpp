#include "SemanticAnalyzer.h"
#include "AST.h"
#include "Doctor.h"
#include "Objects.h"
#include "Statement.h"
#include "SymbolScope.h"
#include "Workspace.h"

#include <ctime>
#include <memory>
#include <string>
#include <unordered_map>
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

void SemanticAnalyzer::run(const std::vector<Module_ptr>& build_order)
{
    enter_scope(ScopeType::WORKSPACE);
    register_natives();

    for (const auto mod : build_order)
    {
        enter_scope(ScopeType::MODULE);
        visit(mod->stmts);
        setup_exports(mod);
        leave_scope();

        extract_module_type(mod);
    }

    leave_scope();
}

void SemanticAnalyzer::setup_exports(Module_ptr mod)
{
    SymbolVector result;
    result.reserve(current_scope->symbols.size());

    for (const auto& [name, symbol] : current_scope->symbols)
    {
        if (symbol->is_exported())
        {
            Doctor::get().fatal_if_nullptr(
                symbol->get_type(),
                WaspStage::Semantics,
                "Symbol '" + name + "' has no type information"
            );

            mod->exports.push_back(symbol);
        }
    }
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

void SemanticAnalyzer::extract_module_type(Module_ptr module)
{
    ObjectStringMap members;
    StringVector ordered_keys;

    for (const auto& symbol : module->exports)
    {
        Doctor::get().fatal_if_nullptr(
            symbol->get_type(),
            WaspStage::Semantics,
            "Symbol '" + symbol->name + "' has no type information"
        );

        members[symbol->name] = symbol->get_type();
        ordered_keys.push_back(symbol->name);
    }

    auto module_type = std::make_shared<ModuleType>(
        module->get_name(),
        module->absolute_filepath,
        ordered_keys,
        members
    );

    module->type = make_object(module_type);
}

// ============================================================================
// High Level Visitors
// ============================================================================

void SemanticAnalyzer::visit(std::vector<Statement_ptr>& statements)
{
    hoist_statements(statements);

    for (const auto& stmt : statements)
    {
        visit(stmt);
    }
}

void SemanticAnalyzer::hoist_statements(StatementVector& statements)
{
    for (auto& stmt_ptr : statements)
    {
        std::visit(
            overloaded{
                [&](FunctionDefinition& def)
                {
                    auto [return_type, param_types] = get_function_signature(def);

                    auto signature = make_object(
                        std::make_shared<FunctionType>(param_types, return_type)
                    );

                    auto symbol = SymbolFactory::create_function(
                        def.name,
                        signature,
                        false,
                        current_scope->get_closure_depth(),
                        current_scope->get_lexical_depth()
                    );

                    type_checker->validate_new_function_overload(current_scope, def.name, symbol);

                    def.symbol = symbol;
                    def.group_symbol = current_scope->define(symbol);
                },
                [&](ClassDefinition& class_def)
                {
                    auto symbol = SymbolFactory::create_class(
                        class_def.name,
                        nullptr,
                        current_scope->get_closure_depth(),
                        current_scope->get_lexical_depth()
                    );

                    current_scope->define(symbol);
                    class_def.symbol = symbol;
                },
                [](auto&)
                {
                }
            },
            stmt_ptr->data
        );
    }
}

void SemanticAnalyzer::visit(const Statement_ptr statement)
{
    Doctor::get().fatal_if_nullptr(statement, WaspStage::Semantics);

    std::visit(
        overloaded{
            [&](ExpressionStatement& stat)
            {
                visit(stat);
            },
            [&](VariableDefinition& stat)
            {
                visit(stat);
            },
            [&](AliasDefinition& stat)
            {
                visit(stat);
            },
            [&](EnumDefinition& stat)
            {
                visit(stat);
            },
            [&](FunctionDefinition& stat)
            {
                visit(stat);
            },
            [&](ClassDefinition& stat)
            {
                visit(stat);
            },
            [&](AnnotationDefinition& stat)
            {
                visit(stat);
            },
            [&](IfBranch& stat)
            {
                visit(stat);
            },
            [&](ElseBranch& stat)
            {
                visit(stat);
            },
            [&](SimpleLoop& stat)
            {
                visit(stat);
            },
            [&](ForInLoop& stat)
            {
                visit(stat);
            },
            [&](LoopControl& stat)
            {
                visit(stat);
            },
            [&](Pass& stat)
            {
                visit(stat);
            },
            [&](Return& stat)
            {
                visit(stat);
            },
            [&](SimpleImport& stat)
            {
                visit(stat);
            },
            [&](FromImport& stat)
            {
                visit(stat);
            },
            [](auto)
            {
                Doctor::get().Doctor::get().fatal(
                    WaspStage::Semantics,
                    "Unhandled Statement in Semantic Analyzer!"
                );
            }
        },
        statement->data
    );
}

} // namespace Wasp
