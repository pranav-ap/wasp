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
#include <variant>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

void SemanticAnalyzer::hoist_statements(StatementVector& statements)
{
    for (auto& stmt_ptr : statements)
    {
        std::visit(
            overloaded{
                [&](FunctionDefinition& def)
                {
                    auto [return_type, param_types] = get_function_signature(def);

                    auto type = make_object(
                        std::make_shared<Signature>(Signature{param_types, return_type})
                    );

                    auto symbol = SymbolFactory::create_function(
                        def.name,
                        type,
                        false,
                        current_scope->get_closure_depth(),
                        current_scope->get_lexical_depth()
                    );

                    type_checker->validate_new_function_overload(current_scope, def.name, symbol);

                    def.symbol = symbol;
                    def.group_symbol = current_scope->define(symbol);
                },
                [&](ClassDefinition& def)
                {
                    auto symbol = SymbolFactory::create_class(
                        def.name,
                        nullptr,
                        current_scope->get_closure_depth(),
                        current_scope->get_lexical_depth()
                    );

                    def.symbol = current_scope->define(symbol);
                },
                [&](EnumDefinition& def)
                {
                    auto symbol = SymbolFactory::create_enum(
                        def.name,
                        nullptr,
                        current_scope->get_closure_depth(),
                        current_scope->get_lexical_depth()
                    );

                    def.symbol = current_scope->define(symbol);
                },
                [&](TypeAliasDefinition& def)
                {
                    auto symbol = SymbolFactory::create_type_alias(
                        def.name,
                        nullptr,
                        current_scope->get_closure_depth(),
                        current_scope->get_lexical_depth()
                    );

                    def.symbol = current_scope->define(symbol);
                },
                [&](TemplateDefinition& template_def)
                {
                    auto target_scope = current_scope;

                    enter_scope(ScopeType::TEMPLATE);

                    ObjectStringMap generics;

                    for (auto& field : template_def.members)
                    {
                        auto constraint_type = field.type ? visit(field.type)
                                                          : workspace->pool->get_any_type();

                        auto generic_type_obj = make_object(
                            std::make_shared<GenericType>(GenericType{field.name, constraint_type})
                        );

                        auto symbol = SymbolFactory::create_generic(field.name, generic_type_obj);
                        current_scope->define(symbol);
                        generics[field.name] = generic_type_obj;
                    }

                    std::visit(
                        overloaded{
                            [&](FunctionDefinition& target_def)
                            {
                                auto [return_type, param_types] = get_function_signature(
                                    target_def
                                );

                                auto signature = std::make_shared<Signature>(
                                    Signature{param_types, return_type}
                                );

                                auto type = make_object(
                                    std::make_shared<TemplateType>(generics, make_object(signature))
                                );

                                auto symbol = SymbolFactory::create_template(
                                    target_def.name,
                                    type,
                                    target_scope->get_closure_depth(),
                                    target_scope->get_lexical_depth()
                                );

                                type_checker->validate_new_function_overload(
                                    target_scope,
                                    target_def.name,
                                    symbol
                                );

                                template_def.symbol = symbol;
                                target_def.symbol = symbol;
                                target_def.group_symbol = target_scope->define(symbol);
                            },
                            [&](ClassDefinition& target_def)
                            {
                                auto class_type = make_object(
                                    std::make_shared<ClassType>(
                                        target_def.name,
                                        ObjectStringMap{},
                                        StringVector{},
                                        StringVector{},
                                        StringVector{},
                                        StringVector{}
                                    )
                                );

                                auto type = make_object(
                                    std::make_shared<TemplateType>(generics, class_type)
                                );

                                auto symbol = SymbolFactory::create_template(
                                    target_def.name,
                                    type,
                                    target_scope->get_closure_depth(),
                                    target_scope->get_lexical_depth()
                                );

                                target_scope->define(symbol);

                                template_def.symbol = symbol;
                                target_def.symbol = symbol;
                            },
                            [&](TypeAliasDefinition& target_def)
                            {
                                auto type = make_object(
                                    std::make_shared<TemplateType>(generics, nullptr)
                                );

                                auto symbol = SymbolFactory::create_template(
                                    target_def.name,
                                    type,
                                    target_scope->get_closure_depth(),
                                    target_scope->get_lexical_depth()
                                );

                                target_scope->define(symbol);

                                template_def.symbol = symbol;
                                target_def.symbol = symbol;
                            },
                            [](auto&)
                            {
                                Doctor::get().fatal(
                                    WaspStage::Semantics,
                                    "Invalid template target"
                                );
                            }
                        },
                        template_def.target->data
                    );

                    leave_scope();
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
