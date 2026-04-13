#include "AST.h"
#include "Doctor.h"
#include "Objects.h"
#include "SemanticAnalyzer.h"
#include "Statement.h"
#include "SymbolScope.h"
#include "Workspace.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <string>
#include <unordered_set>
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

void SemanticAnalyzer::visit(ClassDefinition& class_def)
{
    analyze_membered_definition(class_def, false);
}

void SemanticAnalyzer::visit(TraitDefinition& trait_def)
{
    analyze_membered_definition(trait_def, true);
}

void SemanticAnalyzer::analyze_membered_definition(MemberedDefinition& def, bool is_trait)
{
    ObjectStringMap member_types;
    StringVector declaration_order;
    std::unordered_set<std::string> shared_members;

    for (auto& stmt : def.members)
    {
        std::visit(
            overloaded{
                [&](FieldDefinition& field)
                {
                    member_types[field.name] = visit(field.type);
                    declaration_order.push_back(field.name);

                    if (field.is_our)
                    {
                        shared_members.insert(field.name);
                    }
                },
                [&](MyMethodDefinition& method)
                {
                    if (std::find(
                            declaration_order.begin(),
                            declaration_order.end(),
                            method.name
                        ) == declaration_order.end())
                    {
                        declaration_order.push_back(method.name);
                    }
                },
                [&](OurMethodDefinition& method)
                {
                    if (std::find(
                            declaration_order.begin(),
                            declaration_order.end(),
                            method.name
                        ) == declaration_order.end())
                    {
                        declaration_order.push_back(method.name);
                    }
                    shared_members.insert(method.name);
                },
                [&](auto&)
                {
                    Doctor::get().fatal(
                        WaspStage::Semantics,
                        "Invalid statement in container body."
                    );
                }
            },
            stmt->data
        );
    }

    Object_ptr target_type_obj;
    std::shared_ptr<ContainerType> container_type = nullptr;

    if (is_trait)
    {
        auto trait_type = std::make_shared<TraitType>(
            def.name,
            std::move(member_types),
            std::move(shared_members),
            std::move(declaration_order)
        );

        target_type_obj = make_object(trait_type);
        container_type = trait_type;
    }
    else
    {
        auto class_type = std::make_shared<ClassType>(
            def.name,
            std::move(member_types),
            std::move(shared_members),
            std::move(declaration_order)
        );

        target_type_obj = make_object(class_type);
        container_type = class_type;
    }

    Doctor::get().fatal_if_nullptr(def.symbol, WaspStage::Semantics);
    def.symbol->set_type(target_type_obj);
    container_type_stack.push_back(target_type_obj);

    // Hoist methods
    for (auto& stmt : def.members)
    {
        std::visit(
            overloaded{
                [&](MyMethodDefinition& m)
                {
                    hoist_method(m, false, def.name, target_type_obj, container_type);
                },
                [&](OurMethodDefinition& m)
                {
                    hoist_method(m, true, def.name, target_type_obj, container_type);
                },
                [&](auto&)
                {
                }
            },
            stmt->data
        );
    }

    // Analysis Pass
    for (auto& stmt : def.members)
    {
        std::visit(
            overloaded{
                [&](MyMethodDefinition& m)
                {
                    visit(m);
                },
                [&](OurMethodDefinition& m)
                {
                    visit(m);
                },
                [&](auto&)
                {
                }
            },
            stmt->data
        );
    }

    container_type_stack.pop_back();
}

// ============================================================================
// FUNCTIONS
// ============================================================================

void SemanticAnalyzer::visit(LocalFunctionDefinition& fun_def)
{
    analyze_abstract_function_body(fun_def, false, false);
}

void SemanticAnalyzer::visit(MyMethodDefinition& method_def)
{
    analyze_abstract_function_body(method_def, true, true);
}

void SemanticAnalyzer::visit(OurMethodDefinition& method_def)
{
    analyze_abstract_function_body(method_def, false, true);
}

void SemanticAnalyzer::analyze_abstract_function_body(
    AbstractFunctionDefinition& fun_def,
    bool inject_my,
    bool inject_our
)
{
    Object_ptr return_type;
    ObjectVector param_types;

    if (fun_def.symbol->get_type() == nullptr)
    {
        // Top-level function (SymbolHoister left the type as nullptr)
        auto evaluated = evaluate_function_signature(fun_def);

        // FIX: Assign to the outer variables instead of shadowing them!
        return_type = evaluated.first;
        param_types = evaluated.second;

        auto signature = make_object(std::make_shared<LocalFunctionType>(param_types, return_type));
        fun_def.symbol->set_type(signature);

        fun_def.group_symbol = current_scope->lookup(fun_def.name);
    }
    else
    {
        // Impl method or local function (already evaluated!)
        Object_ptr type_obj = fun_def.symbol->get_type();

        if (auto ptr = type_obj->try_as<std::shared_ptr<MyMethodType>>())
        {
            return_type = (*ptr)->return_type;
            param_types = (*ptr)->input_types;
        }
        else if (auto ptr = type_obj->try_as<std::shared_ptr<OurMethodType>>())
        {
            return_type = (*ptr)->return_type;
            param_types = (*ptr)->input_types;
        }
        else if (auto ptr = type_obj->try_as<std::shared_ptr<LocalFunctionType>>())
        {
            return_type = (*ptr)->return_type;
            param_types = (*ptr)->input_types;
        }
        else
        {
            Doctor::get().fatal(
                WaspStage::Semantics,
                "Internal Compiler Error: Expected concrete function type."
            );
        }
    }

    type_checker->validate_overload_group(current_scope, fun_def.name, fun_def.symbol);
    Doctor::get().fatal_if_nullptr(fun_def.group_symbol, WaspStage::Semantics);

    enter_scope(ScopeType::FUNCTION);
    return_type_stack.push_back(return_type);
    fun_def.parameter_symbols.clear();

    auto define_param = [&](const std::string& name, Object_ptr type, bool is_mutable)
    {
        auto sym = SymbolFactory::create_variable(
            name,
            type,
            is_mutable,
            current_scope->get_closure_depth(),
            current_scope->get_lexical_depth()
        );

        current_scope->define(sym);
        fun_def.parameter_symbols.push_back(sym);
    };

    if (!container_type_stack.empty())
    {
        Object_ptr current_class = container_type_stack.back();

        if (inject_my)
            define_param("my", current_class, false);
        if (inject_our)
            define_param("our", current_class, false);
    }

    for (size_t i = 0; i < fun_def.parameters.size(); ++i)
    {
        define_param(fun_def.parameters[i].first, param_types[i], true);
    }

    // Mask class context for nested functions
    container_type_stack.push_back(nullptr);

    hoist_statements(fun_def.body);

    for (auto& stmt : fun_def.body)
    {
        visit(stmt);
    }

    container_type_stack.pop_back();
    return_type_stack.pop_back();
    leave_scope();
}

void SemanticAnalyzer::hoist_method(
    AbstractFunctionDefinition& method_def,
    bool is_our,
    const std::string& container_name,
    Object_ptr target_type_obj,
    std::shared_ptr<ContainerType> container_type
)
{
    std::string original_name = method_def.name;
    method_def.name = container_name + "::" + original_name;

    auto [ret_type, param_types] = evaluate_function_signature(method_def);

    Object_ptr signature;
    if (is_our)
    {
        signature = make_object(
            std::make_shared<OurMethodType>(param_types, ret_type, target_type_obj)
        );
    }
    else
    {
        signature = make_object(
            std::make_shared<MyMethodType>(param_types, ret_type, target_type_obj)
        );
    }

    Symbol_ptr method_symbol = is_our ? SymbolFactory::create_our_method(
                                            method_def.name,
                                            signature,
                                            target_type_obj,
                                            false,
                                            current_scope->get_closure_depth(),
                                            current_scope->get_lexical_depth()
                                        )
                                      : SymbolFactory::create_my_method(
                                            method_def.name,
                                            signature,
                                            target_type_obj,
                                            false,
                                            current_scope->get_closure_depth(),
                                            current_scope->get_lexical_depth()
                                        );

    if (current_scope->contains_in_current_scope(method_def.name))
    {
        type_checker->validate_overload_group(current_scope, method_def.name, method_symbol);
    }

    current_scope->define(method_symbol);
    method_def.symbol = method_symbol;
    method_def.group_symbol = current_scope->lookup(method_def.name);

    if (container_type)
    {
        // FIX: Store the explicit method signature, NOT the group symbol signature!
        container_type->members[original_name] = method_symbol->get_type();
    }
}

std::pair<Object_ptr, ObjectVector> SemanticAnalyzer::evaluate_function_signature(
    AbstractFunctionDefinition& func
)
{
    Object_ptr return_type = func.return_type ? visit(func.return_type)
                                              : workspace->pool->get_none_type();
    ObjectVector param_types;

    for (const auto& [param_name, type_ann] : func.parameters)
    {
        param_types.push_back(type_ann ? visit(type_ann) : workspace->pool->get_any_type());
    }

    return {return_type, param_types};
}

void SemanticAnalyzer::visit(Return& statement)
{
    Doctor::get().assert(
        !return_type_stack.empty(),
        WaspStage::Semantics,
        "'return' statement used outside of a function."
    );

    Object_ptr expected = return_type_stack.back();
    Object_ptr actual = statement.expression ? visit(statement.expression.value())
                                             : workspace->pool->get_none_type();

    Doctor::get().assert(
        type_checker->assignable(current_scope, expected, actual),
        WaspStage::Semantics,
        "Return type mismatch. Expected " + stringify_object(expected) + ", got " +
            stringify_object(actual)
    );
}

// -------------------------------------------------------------------
// Other Visitors
// -------------------------------------------------------------------

void SemanticAnalyzer::visit(AliasDefinition& statement)
{
}
void SemanticAnalyzer::visit(EnumDefinition& statement)
{
}
void SemanticAnalyzer::visit(AnnotationDefinition& statement)
{
}

} // namespace Wasp
