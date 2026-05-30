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
                    hoist(stmt);
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
    for (auto& stmt_ptr : statements)
    {
        std::visit(
            overloaded{
                [&](TypeAliasDefinition& def)
                {
                    auto alias_type = def.symbol->get_type()
                                          ->as<TypeAlias_ptr>();

                    alias_type->template_type = create_template_type(
                        def.template_params
                    );

                    enter_scope(ScopeType::CLASS);

                    define_template_parameters(alias_type->template_type);
                    alias_type->underlying_type = visit(def.ref_type);

                    leave_scope();
                },
                [&](ClassDefinition& def)
                {
                    auto class_type = def.symbol->get_type()
                                          ->as<ClassType_ptr>();

                    class_type->template_type = create_template_type(
                        def.template_params
                    );

                    auto& class_data = def.symbol->as<OopsSymbol>();

                    ASTCloner cloner;
                    class_data.definition = cloner.clone(make_statement(def));
                    class_data.declaration_scope = current_scope;
                },
                [&](TraitDefinition& def)
                {
                    auto trait_type = def.symbol->get_type()
                                          ->as<TraitType_ptr>();

                    trait_type->template_type = create_template_type(
                        def.template_params
                    );

                    auto& trait_data = def.symbol->as<OopsSymbol>();

                    ASTCloner cloner;
                    trait_data.definition = cloner.clone(make_statement(def));
                    trait_data.declaration_scope = current_scope;
                },
                [&](FunctionDefinition& def)
                {
                    hoist(def);

                    auto& function_data = def.symbol->as<FunctionSymbol>();

                    ASTCloner cloner;
                    function_data.definition = cloner.clone(
                        make_statement(def)
                    );
                    function_data.declaration_scope = current_scope;
                },
                [](auto&)
                { /* No signature hoisting needed for other statements */ }
            },
            stmt_ptr->data
        );
    }
}

void SemanticAnalyzer::hoist(Import& stmt)
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
            "Module '" + mod->get_name() +
                "' does not export symbol: " + pair.name
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

void SemanticAnalyzer::hoist(CallableDefinition& def)
{
    enter_scope(ScopeType::FUNCTION);

    auto template_type = create_template_type(def.template_params);
    define_template_parameters(template_type);

    Object_ptr return_type = def.return_type ? visit(def.return_type)
                                             : workspace->pool->get_none_type();

    ObjectVector param_types;
    for (const auto& param : def.parameters)
    {
        param_types.push_back(visit(param.type));
    }

    leave_scope();

    auto signature = make_object(
        std::make_shared<Signature>(param_types, return_type, template_type)
    );

    auto function_symbol = SymbolFactory::create_function(
        def.name,
        signature,
        current_scope->get_closure_depth(),
        current_scope->get_lexical_depth()
    );

    define_or_add_to_overload_set(def.name, function_symbol);

    def.symbol = function_symbol;
    def.group_symbol = current_scope->lookup(def.name);
}

void SemanticAnalyzer::define_or_add_to_overload_set(
    const std::string& name,
    Symbol_ptr new_function
)
{
    auto existing = current_scope->lookup(name);

    if (!existing)
    {
        auto overload_set = SymbolFactory::create_overloads(
            name,
            current_scope->get_closure_depth(),
            current_scope->get_lexical_depth()
        );

        overload_set->add_overload(new_function);
        current_scope->define(overload_set);

        return;
    }

    Doctor::get().assert(
        existing->is<OverloadsSymbol>(),
        WaspStage::Semantics,
        "'" + name + "' already exists and is not a function"
    );

    auto& overload_symbol = existing->as<OverloadsSymbol>();
    auto new_signature = new_function->get_type()->as<Signature_ptr>();

    validate_unique_signature(overload_symbol, new_signature, name);

    overload_symbol.overloads.push_back(new_function);
}

void SemanticAnalyzer::validate_unique_signature(
    const OverloadsSymbol& overload_symbol,
    const Signature_ptr& new_signature,
    const std::string& function_name
)
{
    for (const auto& existing_func : overload_symbol.overloads)
    {
        auto existing_sig = existing_func->get_type()->as<Signature_ptr>();

        Doctor::get().assert(
            !type_system->equal(
                current_scope,
                existing_sig->parameter_types,
                new_signature->parameter_types
            ),
            WaspStage::Semantics,
            "Duplicate function signature for " + function_name
        );
    }
}

} // namespace Wasp
