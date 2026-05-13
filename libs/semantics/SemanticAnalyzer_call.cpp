#include "AST.h"
#include "Doctor.h"
#include "Expression.h"
#include "Objects.h"
#include "SemanticAnalyzer.h"
#include "SymbolScope.h"
#include "Workspace.h"

#include <cctype>
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

                return std::visit(
                    overloaded{
                        [&](ClassType_ptr class_type) -> Object_ptr
                        {
                            return call_method(call, ma, argument_types, class_type);
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
                        [&](TemplateParameterType_ptr template_param_type)
                            -> Object_ptr
                        {
                            return call_template_method(
                                call,
                                ma,
                                argument_types,
                                template_param_type
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

Object_ptr SemanticAnalyzer::resolve_standard_overload(
    Call& call,
    Symbol_ptr overload_symbol,
    const ObjectVector& argument_types
)
{
    Doctor::get().assert(
        overload_symbol->payload_is<OverloadsData>(),
        WaspStage::Semantics,
        "Symbol must hold function overloads."
    );

    auto candidates = overload_symbol->get_overloads();

    auto [function_symbol, overload_index] = type_system->get_best_function_symbol(
        current_scope,
        candidates,
        argument_types
    );

    call.overload_index = overload_index;
    return function_symbol->get_type()->as<Signature_ptr>()->return_type;
}

Object_ptr SemanticAnalyzer::call_method(
    Call& call,
    MemberAccess& access,
    const ObjectVector& argument_types,
    ClassType_ptr class_type
)
{
    auto method_name = access.right->as<Identifier>().name;

    Doctor::get().assert(
        class_type->contains_member(method_name),
        WaspStage::Semantics,
        "Method '" + method_name + "()' does not exist on class '" +
            class_type->name + "'."
    );

    auto member = class_type->get_member(method_name);

    Doctor::get().assert(
        member->is<ObjectOverloadList_ptr>(),
        WaspStage::Semantics,
        "Member '" + method_name + "' must be an object overload group."
    );

    const auto& overloads = member->as<ObjectOverloadList_ptr>()->overloads;

    auto [signature_obj, overload_index] = type_system->get_best_function_object(
        current_scope,
        overloads,
        argument_types
    );

    access.member_index = class_type->get_member_index(method_name);
    call.overload_index = overload_index;

    call.is_method_call = true;

    return signature_obj->as<Signature_ptr>()->return_type;
}

Object_ptr SemanticAnalyzer::call_template_function(
    Call& call,
    TemplateAngular& template_angular,
    const ObjectVector& argument_types
)
{
    // Evaluate Angular Arguments - greet<int, str>

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

    // Find Best Specialization

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

    call.overload_index = original_indices[subset_index];

    return best_signature_object->as<Signature_ptr>()->return_type;
}

Object_ptr SemanticAnalyzer::call_template_method(
    Call& call,
    MemberAccess& access,
    const ObjectVector& argument_types,
    TemplateParameterType_ptr template_parameter_type
)
{
    Doctor::get().fatal(
        WaspStage::Semantics,
        "Calling template methods is not yet implemented."
    );
}

} // namespace Wasp
