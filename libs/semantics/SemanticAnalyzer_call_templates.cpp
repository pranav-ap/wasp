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
#include <vector>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

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

    // Check if this exact specialization already exists
    for (const auto& existing_func : overload_symbol.overloads)
    {
        auto existing_sig = existing_func->get_type()->as<Signature_ptr>();

        if (type_system->equal(
                current_scope,
                existing_sig->parameter_types,
                new_signature->parameter_types
            ))
        {
            // Already exists. So don't add duplicate
            return;
        }
    }

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
        Doctor::get().assert(
            substitutions.contains(param),
            WaspStage::Semantics,
            "Could not deduce template parameter '" + param +
                "'. Explicit template arguments are required."
        );

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
    auto ast = forest[blueprint_symbol];
    Doctor::get().fatal_if_nullptr(ast, WaspStage::Semantics);

    ASTCloner cloner(substitutions);
    Statement_ptr specialized_stmt = cloner.clone(ast);

    Symbol_ptr specialized_group_symbol = nullptr;
    // SymbolScope_ptr previous_scope = current_scope;
    // current_scope = function_data.declaration_scope;

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

    // current_scope = previous_scope;
    pending_templates.push_back(specialized_stmt);

    return specialized_group_symbol;
}

} // namespace Wasp
