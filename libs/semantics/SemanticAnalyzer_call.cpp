#include "AST.h"
#include "ASTCloner.h"
#include "Doctor.h"
#include "Expression.h"
#include "Objects.h"
#include "SemanticAnalyzer.h"
#include "Statement.h"
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
                return handle_identifier_call(call, id, argument_types);
            },
            [&](TemplateAngular& ta)
            {
                return call_template_function(call, ta, argument_types);
            },
            [&](MemberAccess& ma)
            {
                return handle_member_call(call, ma, argument_types);
            },
            [&](auto&) -> Object_ptr
            {
                Doctor::get().fatal(WaspStage::Semantics, "Invalid callable");
            }
        },
        call.callable->data
    );
}

// ============================================================================
// Call Handlers
// ============================================================================

Object_ptr SemanticAnalyzer::handle_identifier_call(
    Call& call,
    Identifier& id,
    const ObjectVector& argument_types
)
{
    auto symbol = current_scope->lookup_required_and_resolve(id.name);
    bind_identifier(id, symbol);
    return resolve_standard_overload(call, symbol, argument_types);
}

Object_ptr SemanticAnalyzer::handle_member_call(
    Call& call,
    MemberAccess& ma,
    ObjectVector& argument_types
)
{
    Object_ptr left_type = visit(ma.left)->unwrap_completely();
    argument_types.insert(argument_types.begin(), left_type);

    return std::visit(
        overloaded{
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
            [&](auto&) -> Object_ptr
            {
                Doctor::get().fatal(
                    WaspStage::Semantics,
                    "Invalid member call LHS"
                );
            }
        },
        left_type->value
    );
}

// ============================================================================
// Standard Overload Resolution
// ============================================================================

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
    box_arguments_if_needed(call, argument_types, signature);

    if (signature->template_type->exists())
    {
        return resolve_implicit_template(
            call,
            function_symbol,
            signature,
            argument_types
        );
    }

    call.overload_index = compute_runtime_overload_index(
        candidates,
        function_symbol
    );

    return signature->return_type;
}

int SemanticAnalyzer::compute_runtime_overload_index(
    const SymbolVector& candidates,
    Symbol_ptr winner
)
{
    int index = 0;

    for (const auto& candidate : candidates)
    {
        if (candidate == winner)
        {
            break;
        }

        auto sig = candidate->get_type()->as<Signature_ptr>();

        if (sig->template_type->exists())
        {
            index++;
        }
    }

    return index;
}

// ============================================================================
// Implicit Template Resolution
// ============================================================================

Object_ptr SemanticAnalyzer::resolve_implicit_template(
    Call& call,
    Symbol_ptr function_symbol,
    Signature_ptr signature,
    const ObjectVector& argument_types
)
{
    Doctor::get().assert(
        signature->template_type != nullptr,
        WaspStage::Semantics,
        "Expected template_type for implicit template resolution"
    );

    auto substitutions = type_system->infer_template_arguments(
        signature,
        argument_types
    );
    auto deduced_args = validate_and_collect_deduced_args(
        signature,
        substitutions
    );

    std::string specialized_name = function_symbol->name + "_" +
                                   Object::mangle_object(deduced_args);

    auto specialized_group = monomorphize_callable_template(
        function_symbol,
        substitutions,
        specialized_name
    );

    call.overload_index = 0;
    replace_callable_with_specialized(
        call,
        specialized_name,
        specialized_group
    );

    auto overloads = specialized_group->get_overloads();
    return overloads[0]
        ->get_type()
        ->as<Signature_ptr>()
        ->return_type->unwrap_completely();
}

ObjectVector SemanticAnalyzer::validate_and_collect_deduced_args(
    const Signature_ptr& signature,
    const ObjectStringMap& substitutions
)
{
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
        deduced_args.push_back(substitutions.at(param));
    }
    return deduced_args;
}

void SemanticAnalyzer::replace_callable_with_specialized(
    Call& call,
    const std::string& specialized_name,
    Symbol_ptr specialized_group
)
{
    Identifier specialized_id(specialized_name);
    specialized_id.symbol = specialized_group;
    call.callable->data = specialized_id;
}

// ============================================================================
// Template Function Call with Explicit Angular Arguments
// ============================================================================

Object_ptr SemanticAnalyzer::call_template_function(
    Call& call,
    TemplateAngular& template_angular,
    const ObjectVector& argument_types
)
{
    auto angular_args = resolve_angular_arguments(template_angular);
    auto target_symbol = resolve_template_target(template_angular);

    auto candidates = target_symbol->get_overloads();
    auto generic_candidates = type_system->filter_by_generic_arity(
        candidates,
        angular_args.size()
    );

    auto
        [specialized_candidates,
         original_indices] = type_system
                                 ->specialize_candidates(
                                     generic_candidates,
                                     angular_args
                                 );

    auto [best_signature, subset_index] = type_system->get_best_function_object(
        current_scope,
        specialized_candidates,
        argument_types
    );

    auto blueprint_symbol = candidates[original_indices[subset_index]];
    auto substitutions = build_substitutions_from_angular_args(
        blueprint_symbol,
        angular_args
    );

    std::string specialized_name = blueprint_symbol->name + "_" +
                                   Object::mangle_object(angular_args);

    auto specialized_group = monomorphize_callable_template(
        blueprint_symbol,
        substitutions,
        specialized_name
    );

    template_angular.symbol = specialized_group;
    call.overload_index = 0;

    replace_callable_with_specialized(
        call,
        specialized_name,
        specialized_group
    );

    auto overloads = specialized_group->get_overloads();
    return overloads[0]
        ->get_type()
        ->as<Signature_ptr>()
        ->return_type->unwrap_completely();
}

ObjectVector SemanticAnalyzer::resolve_angular_arguments(
    TemplateAngular& template_angular
)
{
    ObjectVector args;
    for (auto& node : template_angular.angular_nodes)
    {
        args.push_back(visit(node));
    }
    return args;
}

Symbol_ptr SemanticAnalyzer::resolve_template_target(
    TemplateAngular& template_angular
)
{
    auto symbol = resolve_target_symbol(template_angular.target);
    template_angular.symbol = symbol;

    Doctor::get().assert(
        template_angular.target->is<Identifier>(),
        WaspStage::Semantics,
        "Expected template target to be an identifier."
    );

    bind_identifier(template_angular.target->as<Identifier>(), symbol);
    return symbol;
}

ObjectStringMap SemanticAnalyzer::build_substitutions_from_angular_args(
    Symbol_ptr blueprint_symbol,
    const ObjectVector& angular_args
)
{
    auto signature = blueprint_symbol->get_type()->as<Signature_ptr>();
    Doctor::get().fatal_if_nullptr(
        signature->template_type,
        WaspStage::Semantics,
        "Expected template_type on function signature"
    );

    ObjectStringMap substitutions;
    const auto& param_names = signature->template_type->ordered_parameter_names;
    for (size_t i = 0; i < param_names.size(); ++i)
    {
        substitutions[param_names[i]] = angular_args[i];
    }
    return substitutions;
}

// ============================================================================
// Monomorphization
// ============================================================================

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
                hoist(def);
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

    Doctor::get().fatal_if_nullptr(
        specialized_group_symbol,
        WaspStage::Semantics
    );

    current_scope = previous_scope;
    pending_templates.push_back(specialized_stmt);

    return specialized_group_symbol;
}

// ============================================================================
// Method Call
// ============================================================================

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
        "Method '" + method_name + "()' does not exist on class '" +
            oops_type->name + "'."
    );

    auto signatures_set_obj = oops_type->bag_type->get_signatures(method_name);

    Doctor::get().fatal_if_nullptr(
        signatures_set_obj,
        WaspStage::Semantics,
        "Member '" + method_name + "' not found in bag_type"
    );

    Doctor::get().assert(
        signatures_set_obj->is<SignaturesSet_ptr>(),
        WaspStage::Semantics,
        "Member '" + method_name + "' must be a SignaturesSet."
    );

    auto signatures_set = signatures_set_obj->as<SignaturesSet_ptr>();

    auto [best_signature_obj, overload_index] = type_system
                                                    ->get_best_function_object(
                                                        current_scope,
                                                        signatures_set->types,
                                                        argument_types
                                                    );

    access.member_index = oops_type->bag_type->get_index(method_name);

    call.overload_index = overload_index;
    call.is_method_call = true;

    return best_signature_obj->as<Signature_ptr>()->return_type;
}

// ============================================================================
// Boxing Helper
// ============================================================================

void SemanticAnalyzer::box_arguments_if_needed(
    Call& call,
    const ObjectVector& argument_types,
    const Signature_ptr& signature
)
{
    for (size_t i = 0; i < call.arguments.size(); ++i)
    {
        call.arguments[i] = try_box_expression(
            call.arguments[i],
            argument_types[i],
            signature->parameter_types[i]
        );
    }
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
            return make_expression(Box{expr, trait->type_id});
        }
    }
    return expr;
}

} // namespace Wasp
