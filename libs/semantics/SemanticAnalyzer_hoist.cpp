#include "AST.h"
#include "Objects.h"
#include "SemanticAnalyzer.h"
#include "Statement.h"
#include "SymbolScope.h"
#include "Workspace.h"

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
    auto evaluate_generics =
        [&](const std::vector<FieldDefinition>& generic_fields) -> ObjectStringMap
    {
        ObjectStringMap generics_map;

        for (const auto& field : generic_fields)
        {
            auto constraint_type = visit(field.type);

            auto generic_type = make_object(
                std::make_shared<GenericType>(field.name, constraint_type)
            );

            generics_map[field.name] = generic_type;
        }

        return generics_map;
    };

    for (auto& stmt_ptr : statements)
    {
        std::visit(
            overloaded{
                [&](FunctionDefinition& def)
                {
                    ObjectStringMap generics = evaluate_generics(def.generics);
                    Object_ptr return_type = def.return_type
                                                 ? visit(def.return_type)
                                                 : workspace->pool->get_none_type();

                    ObjectVector param_types;

                    for (const auto& [name, type_node] : def.parameters)
                    {
                        param_types.push_back(visit(type_node));
                    }

                    auto signature = make_object(
                        std::make_shared<Signature>(
                            param_types,
                            return_type,
                            generics
                        )
                    );

                    auto symbol = SymbolFactory::create_function(
                        def.name,
                        signature,
                        false,
                        current_scope->get_closure_depth(),
                        current_scope->get_lexical_depth()
                    );

                    type_checker->validate_new_function_overload(
                        current_scope,
                        def.name,
                        symbol
                    );

                    def.symbol = symbol;
                    def.group_symbol = current_scope->define(symbol);
                },
                [&](ClassDefinition& def)
                {
                    ObjectStringMap generics = evaluate_generics(def.generics);

                    auto type = make_object(
                        std::make_shared<ClassType>(
                            def.name,
                            ObjectStringMap{},
                            StringVector{},
                            StringVector{},
                            StringVector{},
                            StringVector{},
                            generics
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
                    ObjectStringMap generics = evaluate_generics(def.generics);

                    auto type = make_object(
                        std::make_shared<TraitType>(
                            def.name,
                            ObjectStringMap{},
                            StringVector{},
                            StringVector{},
                            StringVector{},
                            generics
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
                    ObjectStringMap generics = evaluate_generics(def.generics);

                    auto type_alias_type = std::make_shared<TypeAlias>(
                        def.name,
                        nullptr,
                        generics
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
                    auto type = make_object(std::make_shared<EnumType>(def.name));

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

} // namespace Wasp
