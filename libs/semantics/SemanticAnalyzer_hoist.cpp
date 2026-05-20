#include "AST.h"
#include "ASTCloner.h"
#include "Doctor.h"
#include "Objects.h"
#include "SemanticAnalyzer.h"
#include "Statement.h"
#include "SymbolScope.h"
#include "Workspace.h"

#include <algorithm>
#include <ctime>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

void SemanticAnalyzer::hoist_statements(StatementVector& statements)
{
    hoist_names(statements);
    hoist_signatures(statements);
}

void SemanticAnalyzer::hoist_names(StatementVector& statements)
{
    int closure_depth = current_scope->get_closure_depth();
    int lexical_depth = current_scope->get_lexical_depth();

    for (auto& stmt_ptr : statements)
    {
        std::visit(
            overloaded{
                [&](Import& stmt)
                {
                    hoist_import(stmt);
                },
                [&](ClassDefinition& def)
                {
                    auto type = make_object(
                        std::make_shared<ClassType>(def.name)
                    );

                    def.symbol = current_scope->define(
                        SymbolFactory::create_oops(
                            def.name,
                            type,
                            closure_depth,
                            lexical_depth
                        )
                    );
                },
                [&](TraitDefinition& def)
                {
                    auto type = make_object(
                        std::make_shared<TraitType>(def.name)
                    );

                    def.symbol = current_scope->define(
                        SymbolFactory::create_oops(
                            def.name,
                            type,
                            closure_depth,
                            lexical_depth
                        )
                    );
                },
                [&](TypeAliasDefinition& def)
                {
                    auto type = make_object(
                        std::make_shared<TypeAlias>(def.name)
                    );

                    def.symbol = current_scope->define(
                        SymbolFactory::create_type_alias(
                            def.name,
                            type,
                            closure_depth,
                            lexical_depth
                        )
                    );
                },
                [&](EnumDefinition& def)
                {
                    auto type = make_object(
                        std::make_shared<EnumType>(def.name)
                    );

                    def.symbol = current_scope->define(
                        SymbolFactory::create_enum(
                            def.name,
                            type,
                            closure_depth,
                            lexical_depth
                        )
                    );
                },
                [](auto&)
                { /* Functions and Operators are hoisted in the next pass */ }
            },
            stmt_ptr->data
        );
    }
}

void SemanticAnalyzer::hoist_signatures(StatementVector& statements)
{
    auto assign_generics = [&](auto& def, auto type_ptr)
    {
        auto [template_params, ordered_names] = evaluate_template_params(
            def.template_params
        );

        type_ptr->template_parameter_types = std::move(template_params);
        type_ptr->ordered_template_parameter_names = std::move(ordered_names);
    };

    for (auto& stmt_ptr : statements)
    {
        std::visit(
            overloaded{
                [&](TypeAliasDefinition& def)
                {
                    auto alias_type = def.symbol->get_type()->as<TypeAlias_ptr>();
                    assign_generics(def, alias_type);

                    enter_scope(ScopeType::CLASS);

                    for (const auto& name : alias_type->ordered_template_parameter_names)
                    {
                        auto generic_type = alias_type->template_parameter_types.at(name);
                        auto symbol = SymbolFactory::create_template_parameter(name, generic_type);
                        current_scope->define(symbol);
                    }

                    alias_type->underlying_type = visit(def.ref_type);
                    leave_scope();
                },
                [&](ClassDefinition& def)
                {
                    assign_generics(
                        def,
                        def.symbol->get_type()->as<ClassType_ptr>()
                    );

                    auto& class_data = def.symbol->get_payload_as<OopsData>();

                    ASTCloner cloner;
                    class_data.definition = cloner.clone(make_statement(def));
                    class_data.declaration_scope = current_scope;
                },
                [&](TraitDefinition& def)
                {
                    assign_generics(
                        def,
                        def.symbol->get_type()->as<TraitType_ptr>()
                    );

                    auto& trait_data = def.symbol->get_payload_as<OopsData>();

                    ASTCloner cloner;
                    trait_data.definition = cloner.clone(make_statement(def));
                    trait_data.declaration_scope = current_scope;
                },
                [&](FunctionDefinition& def)
                {
                    hoist_function_definition(def);

                    auto& function_data = def.symbol->get_payload_as<CallableData>();

                    ASTCloner cloner;
                    function_data.definition = cloner.clone(make_statement(def));
                    function_data.declaration_scope = current_scope;
                },
                [&](OperatorDefinition& def)
                {
                    hoist_function_definition(def);

                    auto& function_data = def.symbol->get_payload_as<CallableData>();

                    ASTCloner cloner;
                    function_data.definition = cloner.clone(make_statement(def));
                    function_data.declaration_scope = current_scope;
                },
                [](auto&) { /* No signature hoisting needed for other statements */ }
            },
            stmt_ptr->data
        );
    }
}

void SemanticAnalyzer::hoist_import(Import& stmt)
{
    auto mod = workspace->get_module(stmt.absolute_path);
    Doctor::get().fatal_if_nullptr(mod, WaspStage::Semantics);

    Symbol_ptr module_symbol = SymbolFactory::create_module(
        mod->get_name(),
        mod
    );

    std::string module_path = mod->get_path();

    for (auto& exported_symbol : mod->exports)
    {
        exported_symbol->module_path = module_path;
    }

    if (stmt.module_alias.has_value())
    {
        Symbol_ptr alias_symbol = SymbolFactory::create_symbol_alias(
            stmt.module_alias.value(),
            module_symbol
        );
        current_scope->define(alias_symbol);
        stmt.symbol = alias_symbol;
    }
    else if (!stmt.expose_all && stmt.exposed_symbols.empty())
    {
        stmt.symbol = current_scope->define(module_symbol);
    }
    else
    {
        stmt.symbol = module_symbol;
    }

    for (auto& pair : stmt.exposed_symbols)
    {
        Symbol_ptr exported_symbol = mod->get_member(pair.name);

        Doctor::get().fatal_if_nullptr(
            exported_symbol,
            WaspStage::Semantics,
            "Module '" + mod->get_name() + "' does not export symbol: " + pair.name
        );

        if (pair.alias.has_value())
        {
            Symbol_ptr alias_symbol = SymbolFactory::create_symbol_alias(
                pair.alias.value(),
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

    if (stmt.expose_all)
    {
        for (const auto& exported_symbol : mod->exports)
        {
            bool not_excluded = std::find(
                                    stmt.excluded_symbols.begin(),
                                    stmt.excluded_symbols.end(),
                                    exported_symbol->name
                                ) == stmt.excluded_symbols.end();

            if (not_excluded)
            {
                current_scope->define(exported_symbol);
            }
        }
    }
}

void SemanticAnalyzer::hoist_function_definition(AbstractCallable& def)
{
    enter_scope(ScopeType::FUNCTION);

    auto [generics, ordered_names] = evaluate_template_params(def.template_params);

    for (const auto& [name, generic_type] : generics)
    {
        auto symbol = SymbolFactory::create_template_parameter(name, generic_type);
        current_scope->define(symbol);
    }

    Object_ptr return_type = def.return_type ? visit(def.return_type)
                                             : workspace->pool->get_none_type();

    ObjectVector param_types;
    for (const auto& [name, type_node] : def.parameters)
    {
        param_types.push_back(visit(type_node));
    }

    leave_scope();

    auto signature = make_object(
        std::make_shared<Signature>(param_types, return_type, generics, ordered_names)
    );

    auto symbol = SymbolFactory::create_function(
        def.name,
        signature,
        current_scope->get_closure_depth(),
        current_scope->get_lexical_depth()
    );

    type_system
        ->validate_new_function_overload(current_scope, def.name, symbol);

    def.symbol = symbol;
    def.group_symbol = current_scope->define(symbol);
}

} // namespace Wasp
