#include "AST.h"
#include "Doctor.h"
#include "Objects.h"
#include "SemanticAnalyzer.h"
#include "Statement.h"
#include "SymbolScope.h"
#include "Workspace.h"

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

void SemanticAnalyzer::hoist_names_and_imports(StatementVector& statements)
{
    for (auto& stmt_ptr : statements)
    {
        std::visit(
            overloaded{
                [&](SimpleImport& stmt)
                {
                    auto mod = workspace->get_module(stmt.absolute_path);
                    Doctor::get().fatal_if_nullptr(mod, WaspStage::Semantics);

                    Symbol_ptr module_symbol = SymbolFactory::create_module(
                        mod->get_name(),
                        mod
                    );

                    for (auto& exported_symbol : mod->exports)
                    {
                        exported_symbol->module_path = mod->get_path();
                    }

                    if (stmt.alias.has_value())
                    {
                        std::string alias_name = stmt.alias.value();
                        Symbol_ptr alias_symbol = SymbolFactory::create_alias(
                            alias_name,
                            module_symbol
                        );

                        current_scope->define(alias_symbol);
                        stmt.symbol = alias_symbol;
                    }
                    else
                    {
                        stmt.symbol = current_scope->define(module_symbol);
                    }
                },
                [&](FromImport& stmt)
                {
                    auto mod = workspace->get_module(stmt.absolute_path);
                    Doctor::get().fatal_if_nullptr(mod, WaspStage::Semantics);

                    stmt.symbol = SymbolFactory::create_module(
                        mod->get_name(),
                        mod
                    );
                    std::string module_path = mod->get_path();

                    for (auto& pair : stmt.import_as_pairs)
                    {
                        Symbol_ptr exported_symbol = mod->get_member(pair.name);

                        Doctor::get().assert(
                            exported_symbol != nullptr,
                            WaspStage::Semantics,
                            "Module '" + mod->get_name() +
                                "' does not export symbol: " + pair.name
                        );

                        exported_symbol->module_path = module_path;

                        if (pair.alias.has_value())
                        {
                            std::string alias_name = pair.alias.value();
                            Symbol_ptr
                                alias_symbol = SymbolFactory::create_alias(
                                    alias_name,
                                    exported_symbol
                                );

                            alias_symbol->module_path = module_path;

                            current_scope->define(alias_symbol);
                            pair.symbol = alias_symbol;
                        }
                        else
                        {
                            pair.symbol = current_scope->define(
                                exported_symbol
                            );
                        }
                    }
                },
                [&](ClassDefinition& def)
                {
                    auto type = make_object(
                        std::make_shared<ClassType>(
                            def.name,
                            ObjectStringMap{},
                            StringVector{},
                            StringVector{},
                            StringVector{},
                            StringVector{},
                            ObjectStringMap{},
                            StringVector{}
                        )
                    );

                    auto symbol = SymbolFactory::create_class(
                        def.name,
                        type,
                        current_scope->get_closure_depth(),
                        current_scope->get_lexical_depth()
                    );

                    def.symbol = current_scope->define(symbol);
                },
                [&](TraitDefinition& def)
                {
                    auto type = make_object(
                        std::make_shared<TraitType>(
                            def.name,
                            ObjectStringMap{},
                            StringVector{},
                            StringVector{},
                            StringVector{},
                            StringVector{},
                            ObjectStringMap{},
                            StringVector{}
                        )
                    );

                    auto symbol = SymbolFactory::create_trait(
                        def.name,
                        type,
                        current_scope->get_closure_depth(),
                        current_scope->get_lexical_depth()
                    );

                    def.symbol = current_scope->define(symbol);
                },
                [&](TypeAliasDefinition& def)
                {
                    auto type_alias_type = std::make_shared<TypeAlias>(
                        def.name,
                        nullptr,
                        ObjectStringMap{},
                        StringVector{}
                    );

                    auto symbol = SymbolFactory::create_type_alias(
                        def.name,
                        make_object(type_alias_type),
                        current_scope->get_closure_depth(),
                        current_scope->get_lexical_depth()
                    );

                    def.symbol = current_scope->define(symbol);
                },
                [&](EnumDefinition& def)
                {
                    auto type = make_object(
                        std::make_shared<EnumType>(def.name)
                    );

                    auto symbol = SymbolFactory::create_enum(
                        def.name,
                        type,
                        current_scope->get_closure_depth(),
                        current_scope->get_lexical_depth()
                    );

                    def.symbol = current_scope->define(symbol);
                },
                [](auto&)
                {
                }
            },
            stmt_ptr->data
        );
    }
}

void SemanticAnalyzer::hoist_signatures_and_generics(
    StatementVector& statements
)
{
    auto evaluate_generics =
        [&](
            const std::vector<FieldDefinition>& generic_fields
        ) -> std::pair<ObjectStringMap, StringVector>
    {
        ObjectStringMap generics_map;
        StringVector ordered_names;

        for (const auto& field : generic_fields)
        {
            auto constraint_type = visit(field.type);

            auto generic_type = make_object(
                std::make_shared<GenericType>(field.name, constraint_type)
            );

            generics_map[field.name] = generic_type;
            ordered_names.push_back(field.name);
        }

        return {generics_map, ordered_names};
    };

    for (auto& stmt_ptr : statements)
    {
        std::visit(
            overloaded{
                [&](ClassDefinition& def)
                {
                    auto [generics, ordered_names] = evaluate_generics(
                        def.generics
                    );

                    auto class_type = def.symbol->get_type()
                                          ->as<ClassType_ptr>();
                    class_type->generics = generics;
                    class_type->expected_generic_names_order = ordered_names;
                },
                [&](TraitDefinition& def)
                {
                    auto [generics, ordered_names] = evaluate_generics(
                        def.generics
                    );

                    auto trait_type = def.symbol->get_type()
                                          ->as<TraitType_ptr>();
                    trait_type->generics = generics;
                    trait_type->expected_generic_names_order = ordered_names;
                },
                [&](TypeAliasDefinition& def)
                {
                    auto [generics, ordered_names] = evaluate_generics(
                        def.generics
                    );

                    auto alias_type = def.symbol->get_type()
                                          ->as<TypeAlias_ptr>();
                    alias_type->generics = generics;
                    alias_type->expected_generic_names_order = ordered_names;
                    alias_type->underlying_type = visit(def.ref_type);
                },
                [&](FunctionDefinition& def)
                {
                    auto [generics, ordered_names] = evaluate_generics(
                        def.generics
                    );

                    bool has_generics = prepare_generic_scope(generics);

                    Object_ptr return_type = def.return_type
                                                 ? visit(def.return_type)
                                                 : workspace->pool
                                                       ->get_none_type();

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

                    type_system->validate_new_function_overload(
                        current_scope,
                        def.name,
                        symbol
                    );

                    def.symbol = symbol;
                    def.group_symbol = current_scope->define(symbol);
                },
                [&](OperatorDefinition& def)
                {
                    auto [generics, ordered_names] = evaluate_generics(
                        def.generics
                    );

                    bool has_generics = prepare_generic_scope(generics);

                    Object_ptr return_type = def.return_type
                                                 ? visit(def.return_type)
                                                 : workspace->pool
                                                       ->get_none_type();

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

                    type_system->validate_new_function_overload(
                        current_scope,
                        def.name,
                        symbol
                    );

                    def.symbol = symbol;
                    def.group_symbol = current_scope->define(symbol);
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
