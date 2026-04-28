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
#include <variant>
#include <vector>

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

        hoist_statements(mod->stmts);

        for (const auto& stmt : mod->stmts)
        {
            visit(stmt);
        }

        StringVector ordered_export_names = setup_ordered_export_names(mod);
        setup_exports(mod, ordered_export_names);

        leave_scope();

        extract_module_type(mod);
    }

    leave_scope();
}

StringVector SemanticAnalyzer::setup_ordered_export_names(Module_ptr mod)
{
    StringVector ordered_export_names;

    auto try_add_export = [&](const std::string& name)
    {
        if (std::find(ordered_export_names.begin(), ordered_export_names.end(), name) ==
            ordered_export_names.end())
        {
            ordered_export_names.push_back(name);
        }
    };

    for (auto& stmt_ptr : mod->stmts)
    {
        std::visit(
            overloaded{
                [&](VariableDefinition& def)
                {
                    try_add_export(def.name);
                },
                [&](FunctionDefinition& def)
                {
                    try_add_export(def.name);
                },
                [&](PureFunctionDefinition& def)
                {
                    try_add_export(def.name);
                },
                [&](ClassDefinition& def)
                {
                    try_add_export(def.name);
                },
                [&](TemplateDefinition& def)
                {
                    std::visit(
                        overloaded{
                            [&](FunctionDefinition& f)
                            {
                                try_add_export(f.name);
                            },
                            [&](PureFunctionDefinition& f)
                            {
                                try_add_export(f.name);
                            },
                            [&](ClassDefinition& c)
                            {
                                try_add_export(c.name);
                            },
                            [](auto&)
                            {
                            }
                        },
                        def.target->data
                    );
                },
                [](auto&)
                {
                }
            },
            stmt_ptr->data
        );
    }

    return ordered_export_names;
}

void SemanticAnalyzer::setup_exports(Module_ptr mod, StringVector ordered_export_names)
{
    SymbolVector result;
    result.reserve(ordered_export_names.size());

    for (const auto& name : ordered_export_names)
    {
        auto symbol = current_scope->lookup(name);

        if (symbol && symbol->is_exportable())
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

template <typename T>
void SemanticAnalyzer::hoist_function(T& def, std::shared_ptr<SymbolScope> target_scope)
{
    auto [return_type, param_types] = get_function_signature(def);

    auto type = make_object(std::make_shared<FunctionType>(param_types, return_type));

    auto symbol = SymbolFactory::create_function(
        def.name,
        type,
        false,
        target_scope->get_closure_depth(),
        target_scope->get_lexical_depth()
    );

    type_checker->validate_new_function_overload(target_scope, def.name, symbol);

    def.symbol = symbol;
    def.group_symbol = target_scope->define(symbol);
}

template <typename T>
void SemanticAnalyzer::hoist_template_function(
    T& def,
    std::shared_ptr<SymbolScope> target_scope,
    ObjectStringMap generics
)
{
    auto [return_type, param_types] = get_function_signature(def);

    auto function_type = std::make_shared<FunctionType>(param_types, return_type);
    auto type = make_object(std::make_shared<FunctionTemplateType>(generics, function_type));

    auto symbol = SymbolFactory::create_template(
        def.name,
        type,
        target_scope->get_closure_depth(),
        target_scope->get_lexical_depth()
    );

    type_checker->validate_new_function_overload(target_scope, def.name, symbol);

    def.symbol = symbol;
    def.group_symbol = target_scope->define(symbol);
}

void SemanticAnalyzer::hoist_class(ClassDefinition& def, std::shared_ptr<SymbolScope> target_scope)
{
    auto symbol = SymbolFactory::create_class(
        def.name,
        nullptr,
        target_scope->get_closure_depth(),
        target_scope->get_lexical_depth()
    );

    def.symbol = target_scope->define(symbol);
}

void SemanticAnalyzer::hoist_trait(TraitDefinition& def, std::shared_ptr<SymbolScope> target_scope)
{
    auto symbol = SymbolFactory::create_trait(
        def.name,
        nullptr,
        target_scope->get_closure_depth(),
        target_scope->get_lexical_depth()
    );

    def.symbol = target_scope->define(symbol);
}

void SemanticAnalyzer::hoist_template_class(
    ClassDefinition& def,
    std::shared_ptr<SymbolScope> target_scope,
    ObjectStringMap generics
)
{
    auto type = make_object(std::make_shared<ClassTemplateType>(generics));

    auto symbol = SymbolFactory::create_template(
        def.name,
        type,
        target_scope->get_closure_depth(),
        target_scope->get_lexical_depth()
    );

    def.symbol = target_scope->define(symbol);
}

void SemanticAnalyzer::hoist_template_trait(
    TraitDefinition& def,
    std::shared_ptr<SymbolScope> target_scope,
    ObjectStringMap generics
)
{
    auto type = make_object(std::make_shared<TraitTemplateType>(generics));

    auto symbol = SymbolFactory::create_template(
        def.name,
        type,
        target_scope->get_closure_depth(),
        target_scope->get_lexical_depth()
    );

    def.symbol = target_scope->define(symbol);
}

void SemanticAnalyzer::hoist_template(
    TemplateDefinition& def,
    std::shared_ptr<SymbolScope> target_scope
)
{
    enter_scope(ScopeType::TEMPLATE);

    ObjectStringMap generics;

    for (auto& field : def.members)
    {
        auto constraint_type = field.type ? visit(field.type) : workspace->pool->get_any_type();
        auto generic_type_obj = make_object(std::make_shared<GenericType>(constraint_type));

        auto symbol = SymbolFactory::create_generic(field.name, generic_type_obj);
        current_scope->define(symbol);
        generics[field.name] = generic_type_obj;
    }

    std::visit(
        overloaded{
            [&](FunctionDefinition& f)
            {
                hoist_template_function(f, target_scope, generics);
            },
            [&](PureFunctionDefinition& f)
            {
                hoist_template_function(f, target_scope, generics);
            },
            [&](ClassDefinition& c)
            {
                hoist_template_class(c, target_scope, generics);
            },
            [](auto&)
            {
            }
        },
        def.target->data
    );

    leave_scope();
}

void SemanticAnalyzer::hoist_statements(StatementVector& statements)
{
    for (auto& stmt_ptr : statements)
    {
        std::visit(
            overloaded{
                [&](FunctionDefinition& def)
                {
                    hoist_function(def, current_scope);
                },
                [&](PureFunctionDefinition& def)
                {
                    hoist_function(def, current_scope);
                },
                [&](ClassDefinition& def)
                {
                    hoist_class(def, current_scope);
                },
                [&](TraitDefinition& def)
                {
                    hoist_trait(def, current_scope);
                },
                [&](TemplateDefinition& def)
                {
                    hoist_template(def, current_scope);
                },
                [](auto&)
                {
                }
            },
            stmt_ptr->data
        );
    }
}

void SemanticAnalyzer::visit(std::vector<Statement_ptr>& statements)
{
    hoist_statements(statements);

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
            [&](PureFunctionDefinition& stat)
            {
                visit(stat);
            },
            [&](ClassDefinition& stat)
            {
                visit(stat);
            },
            [&](TraitDefinition& stat)
            {
                visit(stat);
            },
            [&](TemplateDefinition& stat)
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
            [&](Native& stat)
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
                Doctor::get().fatal(
                    WaspStage::Semantics,
                    "Unhandled Statement in Semantic Analyzer!"
                );
            }
        },
        statement->data
    );
}

} // namespace Wasp
