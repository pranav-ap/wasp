#include "AST.h"
#include "ASTCloner.h"
#include "Doctor.h"
#include "Expression.h"
#include "Objects.h"
#include "SemanticAnalyzer.h"
#include "Statement.h"
#include "SymbolScope.h"
#include "Workspace.h"

#include <cctype>
#include <ctime>
#include <string>
#include <variant>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

Object_ptr SemanticAnalyzer::visit(Call& call)
{
    ObjectVector argument_types = visit(call.arguments);

    return std::visit(
        overloaded{
            [&](Identifier& id)
            {
                auto unresolved = current_scope->lookup(id.name);
                Doctor::get().fatal_if_nullptr(
                    unresolved,
                    WaspStage::Semantics,
                    "Undefined callable: '" + id.name + "'"
                );

                auto symbol = unresolved->resolve();
                bind_identifier(id, symbol);

                return resolve_standard_overload(call, symbol, argument_types);
            },
            [&](TemplateAngular& ta)
            {
                return call_template_function(call, ta, argument_types);
            },
            [&](MemberAccess& ma) -> Object_ptr
            {
                Object_ptr left_type = visit(ma.left);
                left_type = left_type->unwrap_completely();

                return std::visit(
                    overloaded{
                        [&](ClassType_ptr class_type) -> Object_ptr
                        {
                            return call_method(call, ma, argument_types, class_type);
                        },
                        [&](TraitType_ptr trait_type) -> Object_ptr
                        {
                            ma.is_trait_dispatch = true;
                            call.is_trait_dispatch = true;
                            call.trait_type_id = trait_type->type_id;

                            return call_method(call, ma, argument_types, trait_type);
                        },
                        [&](ModuleType_ptr module_type) -> Object_ptr
                        {
                            auto member_symbol = get_module_member_symbol(ma);
                            ma.right->as<Identifier>().symbol = member_symbol;

                            return resolve_standard_overload(
                                call,
                                member_symbol,
                                argument_types
                            );
                        },
                        [&](auto&) -> Object_ptr
                        {
                            Doctor::get().fatal(
                                WaspStage::Semantics,
                                "LHS of call must be a module, class, or template"
                            );
                        }
                    },
                    left_type->value
                );
            },
            [&](auto&) -> Object_ptr
            {
                Doctor::get().fatal(
                    WaspStage::Semantics,
                    "Expected an Identifier, TemplateAngular, or MemberAccess as "
                    "the callable."
                );
            }
        },
        call.callable->data
    );
}

Expression_ptr SemanticAnalyzer::try_box_expression(
    Expression_ptr expr,
    Object_ptr actual_type,
    Object_ptr expected_type
)
{
    if (expected_type->is<TraitType_ptr>() && actual_type->is<ClassType_ptr>())
    {
        if (type_system->assignable(current_scope, expected_type, actual_type))
        {
            auto trait = expected_type->as<TraitType_ptr>();
            int trait_type_id = trait->type_id;
            return make_expression(Box{expr, trait_type_id});
        }
    }

    return expr;
}

Object_ptr SemanticAnalyzer::resolve_standard_overload(
    Call& call,
    Symbol_ptr overload_symbol,
    const ObjectVector& argument_types
)
{
    Doctor::get().assert(
        overload_symbol->is<OverloadsSymbol>(),
        WaspStage::Semantics,
        "Symbol must hold function overloads."
    );

    auto candidates = overload_symbol->get_overloads();

    auto [function_symbol, raw_index] = type_system->get_best_function_symbol(
        current_scope,
        candidates,
        argument_types
    );

    auto signature = function_symbol->get_type()->as<Signature_ptr>();

    for (size_t i = 0; i < call.arguments.size(); ++i)
    {
        call.arguments[i] = try_box_expression(
            call.arguments[i],
            argument_types[i],
            signature->parameter_types[i]
        );
    }

    if (!signature->template_type->ordered_parameter_names.empty())
    {
        return resolve_implicit_template(
            call,
            function_symbol,
            signature,
            argument_types
        );
    }

    // Count number of concrete functions before the winner

    int runtime_index = 0;

    for (const auto& candidate : candidates)
    {
        if (candidate == function_symbol)
        {
            break;
        }

        auto cand_sig = candidate->get_type()->as<Signature_ptr>();

        if (!cand_sig->template_type->ordered_parameter_names.empty())
        {
            runtime_index++;
        }
    }

    call.overload_index = runtime_index;
    return signature->return_type;
}

Object_ptr SemanticAnalyzer::resolve_implicit_template(
    Call& call,
    Symbol_ptr function_symbol,
    Signature_ptr signature,
    const ObjectVector& argument_types
)
{
    ObjectStringMap substitutions = type_system->infer_template_arguments(
        signature,
        argument_types
    );

    ObjectVector deduced_args;

    for (const auto& param : signature->template_type->ordered_parameter_names)
    {
        if (!substitutions.contains(param))
        {
            Doctor::get().fatal(
                WaspStage::Semantics,
                "Could not deduce template parameter '" + param +
                    "'. Explicit template arguments are required."
            );
        }
    }

    for (const auto& name : signature->template_type->ordered_parameter_names)
    {
        deduced_args.push_back(substitutions[name]);
    }

    std::string specialized_name = function_symbol->name + "_" +
                                   Object::mangle_object(deduced_args);

    Symbol_ptr specialized_group_symbol = monomorphize_callable_template(
        function_symbol,
        substitutions,
        specialized_name
    );

    call.overload_index = 0;

    Identifier specialized_id(specialized_name);
    specialized_id.symbol = specialized_group_symbol;
    call.callable->data = specialized_id;

    auto overloads = specialized_group_symbol->get_overloads();

    return overloads[0]
        ->get_type()
        ->as<Signature_ptr>()
        ->return_type->unwrap_completely();
}

Symbol_ptr SemanticAnalyzer::monomorphize_callable_template(
    Symbol_ptr blueprint_symbol,
    const ObjectStringMap& substitutions,
    const std::string& specialized_name
)
{
    auto& function_data = blueprint_symbol->as<FunctionSymbol>();

    ASTCloner cloner(substitutions);
    Statement_ptr specialized_stmt = cloner.clone(function_data.definition);

    Symbol_ptr specialized_group_symbol = nullptr;

    SymbolScope_ptr previous_scope = current_scope;
    current_scope = function_data.declaration_scope;

    std::visit(
        overloaded{
            [&](FunctionDefinition& def)
            {
                def.name = specialized_name;
                def.template_params.clear();
                hoist_function_definition(def);
                visit(def);
                specialized_group_symbol = def.group_symbol;
            },
            [&](auto&)
            {
                Doctor::get().fatal(
                    WaspStage::Semantics,
                    "Expected a function or operator definition"
                );
            }
        },
        specialized_stmt->data
    );

    Doctor::get().fatal_if_nullptr(specialized_group_symbol, WaspStage::Semantics);

    current_scope = previous_scope;
    pending_templates.push_back(specialized_stmt);

    return specialized_group_symbol;
}

Object_ptr SemanticAnalyzer::call_template_function(
    Call& call,
    TemplateAngular& template_angular,
    const ObjectVector& argument_types
)
{
    ObjectVector angular_args;
    for (auto& node : template_angular.angular_nodes)
    {
        angular_args.push_back(visit(node));
    }

    template_angular.symbol = resolve_target_symbol(template_angular.target);

    Doctor::get().assert(
        template_angular.target->is<Identifier>(),
        WaspStage::Semantics,
        "Expected template target to be an identifier."
    );

    bind_identifier(
        template_angular.target->as<Identifier>(),
        template_angular.symbol
    );

    auto candidates = template_angular.symbol->get_overloads();

    auto generic_candidates = type_system->filter_by_generic_arity(
        candidates,
        angular_args.size()
    );

    auto [specialized_candidates, original_indices] = type_system
                                                          ->specialize_candidates(
                                                              generic_candidates,
                                                              angular_args
                                                          );

    auto [best_signature_object, subset_index] = type_system
                                                     ->get_best_function_object(
                                                         current_scope,
                                                         specialized_candidates,
                                                         argument_types
                                                     );

    int winning_index = original_indices[subset_index];
    Symbol_ptr blueprint_symbol = candidates[winning_index];

    Doctor::get().assert(
        blueprint_symbol->is<FunctionSymbol>(),
        WaspStage::Semantics,
        "Resolved template symbol does not contain CallableData."
    );

    // Build the substitutions map
    ObjectStringMap substitutions;
    auto expected_names = blueprint_symbol->get_type()
                              ->as<Signature_ptr>()
                              ->template_type->ordered_parameter_names;

    for (size_t i = 0; i < expected_names.size(); ++i)
    {
        substitutions[expected_names[i]] = angular_args[i];
    }

    // Name Mangling
    std::string specialized_name = blueprint_symbol->name + "_" +
                                   Object::mangle_object(angular_args);

    Symbol_ptr specialized_group_symbol = monomorphize_callable_template(
        blueprint_symbol,
        substitutions,
        specialized_name
    );

    // Update AST Nodes
    template_angular.symbol = specialized_group_symbol;
    call.overload_index = 0;

    Identifier specialized_id(specialized_name);
    specialized_id.symbol = specialized_group_symbol;
    call.callable->data = specialized_id;

    // Get the return type safely
    auto overloads = specialized_group_symbol->get_overloads();
    return overloads[0]
        ->get_type()
        ->as<Signature_ptr>()
        ->return_type->unwrap_completely();
}

Object_ptr SemanticAnalyzer::call_method(
    Call& call,
    MemberAccess& access,
    const ObjectVector& argument_types,
    OopsType_ptr oops_type
)
{
    auto method_name = access.right->as<Identifier>().name;

    Doctor::get().assert(
        oops_type->contains_member(method_name),
        WaspStage::Semantics,
        "Method '" + method_name + "()' does not exist on class '" + oops_type->name + "'."
    );

    auto member = oops_type->bag_type.get_overloads(method_name);

    Doctor::get().assert(
        member->is<Pocket_ptr>(),
        WaspStage::Semantics,
        "Member '" + method_name + "' must be an object overload group."
    );

    const auto& overloads = member->as<Pocket_ptr>()->overloads;

    auto [signature_obj, overload_index] = type_system->get_best_function_object(
        current_scope,
        overloads,
        argument_types
    );

    access.member_index = oops_type->bag_type.get_overloads_index(method_name);
    call.overload_index = overload_index;

    call.is_method_call = true;

    return signature_obj->as<Signature_ptr>()->return_type;
}

} // namespace Wasp
