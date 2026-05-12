#include "AST.h"
#include "Doctor.h"
#include "Objects.h"
#include "SemanticAnalyzer.h"
#include "Statement.h"
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
    hoist_names_and_imports(statements);
    hoist_signatures_and_generics(statements);
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
        Symbol_ptr alias_symbol = SymbolFactory::create_alias(
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

        Doctor::get().assert(
            exported_symbol != nullptr,
            WaspStage::Semantics,
            "Module '" + mod->get_name() +
                "' does not export symbol: " + pair.name
        );

        if (pair.alias.has_value())
        {
            Symbol_ptr alias_symbol = SymbolFactory::create_alias(
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
            if (std::find(
                    stmt.excluded_symbols.begin(),
                    stmt.excluded_symbols.end(),
                    exported_symbol->name
                ) == stmt.excluded_symbols.end())
            {
                current_scope->define(exported_symbol);
            }
        }
    }
}

void SemanticAnalyzer::hoist_names_and_imports(StatementVector& statements)
{
    int c_depth = current_scope->get_closure_depth();
    int l_depth = current_scope->get_lexical_depth();

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
                        SymbolFactory::create_class(
                            def.name,
                            type,
                            c_depth,
                            l_depth
                        )
                    );
                },
                [&](TraitDefinition& def)
                {
                    auto type = make_object(
                        std::make_shared<TraitType>(def.name)
                    );
                    def.symbol = current_scope->define(
                        SymbolFactory::create_trait(
                            def.name,
                            type,
                            c_depth,
                            l_depth
                        )
                    );
                },
                [&](TypeAliasDefinition& def)
                {
                    auto type = make_object(
                        std::make_shared<TypeAlias>(
                            def.name,
                            nullptr,
                            ObjectStringMap{},
                            StringVector{}
                        )
                    );
                    def.symbol = current_scope->define(
                        SymbolFactory::create_type_alias(
                            def.name,
                            type,
                            c_depth,
                            l_depth
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
                            c_depth,
                            l_depth
                        )
                    );
                },
                [](auto&)
                {
                }
            },
            stmt_ptr->data
        );
    }
}

std::pair<ObjectStringMap, StringVector> SemanticAnalyzer::evaluate_generics(
    const std::vector<FieldDefinition>& generic_fields
)
{
    ObjectStringMap generics_map;
    StringVector ordered_names;

    for (const auto& field : generic_fields)
    {
        auto generic_type = make_object(
            std::make_shared<GenericType>(field.name, visit(field.type))
        );
        generics_map[field.name] = generic_type;
        ordered_names.push_back(field.name);
    }

    return {generics_map, ordered_names};
}

template <typename CallableDef>
void SemanticAnalyzer::hoist_callable(CallableDef& def)
{
    auto [generics, ordered_names] = evaluate_generics(def.generics);
    bool has_generics = prepare_generic_scope(generics);

    Object_ptr return_type = def.return_type ? visit(def.return_type)
                                             : workspace->pool->get_none_type();

    ObjectVector param_types;
    for (const auto& [name, type_node] : def.parameters)
    {
        param_types.push_back(visit(type_node));
    }

    if (has_generics)
    {
        leave_scope();
    }

    auto signature = make_object(
        std::make_shared<Signature>(
            param_types,
            return_type,
            generics,
            ordered_names
        )
    );
    auto symbol = SymbolFactory::create_function(
        def.name,
        signature,
        false,
        current_scope->get_closure_depth(),
        current_scope->get_lexical_depth()
    );

    type_system
        ->validate_new_function_overload(current_scope, def.name, symbol);

    def.symbol = symbol;
    def.group_symbol = current_scope->define(symbol);
}

void SemanticAnalyzer::hoist_signatures_and_generics(
    StatementVector& statements
)
{
    auto assign_generics = [&](auto& def, auto type_ptr)
    {
        auto [generics, ordered_names] = evaluate_generics(def.generics);
        type_ptr->generics = std::move(generics);
        type_ptr->expected_generic_names_order = std::move(ordered_names);
    };

    for (auto& stmt_ptr : statements)
    {
        std::visit(
            overloaded{
                [&](ClassDefinition& def)
                {
                    assign_generics(
                        def,
                        def.symbol->get_type()->template as<ClassType_ptr>()
                    );
                },
                [&](TraitDefinition& def)
                {
                    assign_generics(
                        def,
                        def.symbol->get_type()->template as<TraitType_ptr>()
                    );
                },
                [&](FunctionDefinition& def)
                {
                    hoist_callable(def);
                },
                [&](OperatorDefinition& def)
                {
                    hoist_callable(def);
                },
                [&](TypeAliasDefinition& def)
                {
                    auto alias_type = def.symbol->get_type()
                                          ->template as<TypeAlias_ptr>();
                    assign_generics(def, alias_type);
                    alias_type->underlying_type = visit(def.ref_type);
                },
                [](auto&)
                {
                }
            },
            stmt_ptr->data
        );
    }
}

} // namespace Wasp
