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
                return call_concrete_template(call, ta, argument_types);
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
                        [&](GenericType_ptr generic) -> Object_ptr
                        {
                            return call_generic_method(
                                call,
                                ma,
                                argument_types,
                                generic
                            );
                        },
                        [&](auto&) -> Object_ptr
                        {
                            Doctor::get().fatal(
                                WaspStage::Semantics,
                                "LHS of call must be a module, class, or template "
                                "parameter."
                            );
                            return nullptr;
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
                return nullptr;
            }
        },
        call.callable->data
    );
}

Object_ptr SemanticAnalyzer::call_generic_method(
    Call& call,
    MemberAccess& access,
    const ObjectVector& argument_types,
    GenericType_ptr generic
)
{
    std::string method_name = access.right->as<Identifier>().name;

    return std::visit(
        overloaded{
            [&](VariantType& union_type) -> Object_ptr
            {
                ObjectVector all_return_types;

                for (auto& variant : union_type.types)
                {
                    auto class_type = try_unwrap_ptr<ClassType_ptr>(variant);

                    Object_ptr ret_type = call_method(
                        call,
                        access,
                        argument_types,
                        class_type
                    );

                    all_return_types.push_back(ret_type);
                }

                ObjectVector unique_return_types = type_system->remove_duplicates(
                    current_scope,
                    all_return_types
                );

                if (unique_return_types.size() == 1)
                {
                    return unique_return_types[0];
                }

                return make_object(VariantType{unique_return_types});
            },
            [&](ClassType_ptr cls) -> Object_ptr
            {
                return call_method(call, access, argument_types, cls);
            },
            [&](auto&) -> Object_ptr
            {
                Doctor::get().fatal(
                    WaspStage::Semantics,
                    "Generic constraint must resolve to a class or variant type."
                );
            }
        },
        generic->constraint_type->value
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

    auto candidates = overload_symbol->get_payload_as<OverloadsData>()
                          .get_overloads();

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

Object_ptr SemanticAnalyzer::call_concrete_template(
    Call& call,
    TemplateAngular& template_angular,
    const ObjectVector& argument_types
)
{
    ObjectVector concrete_arguments;

    for (const auto& node : template_angular.angular_nodes)
    {
        concrete_arguments.push_back(visit(node));
    }

    Symbol_ptr overload_symbol = resolve_target_symbol(template_angular.target);
    template_angular.symbol = overload_symbol;

    Doctor::get().assert(
        overload_symbol->payload_is<OverloadsData>(),
        WaspStage::Semantics,
        "Symbol '" + overload_symbol->name + "' must hold function overloads."
    );

    auto candidates = overload_symbol->get_payload_as<OverloadsData>()
                          .get_overloads();

    auto generic_candidates = type_system->filter_by_generic_arity(
        candidates,
        concrete_arguments.size()
    );

    Doctor::get().assert(
        !generic_candidates.empty(),
        WaspStage::Semantics,
        "No generic functions with required arity found for '" +
            overload_symbol->name + "'"
    );

    auto [specialized_candidates, original_indices] = type_system
                                                          ->specialize_candidates(
                                                              generic_candidates,
                                                              concrete_arguments
                                                          );

    auto [function_object, subset_index] = type_system->get_best_function_object(
        current_scope,
        specialized_candidates,
        argument_types
    );

    call.overload_index = original_indices[subset_index];

    return function_object->as<Signature_ptr>()->return_type;
}

} // namespace Wasp
