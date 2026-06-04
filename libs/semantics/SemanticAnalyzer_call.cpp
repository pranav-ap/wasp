#include "AST.h"
#include "Doctor.h"
#include "Expression.h"
#include "Objects.h"
#include "SemanticAnalyzer.h"
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

            [&](BooleanType_ptr type) -> Object_ptr
            {
                return call_native_method(call, ma, argument_types, "bool");
            },
            [&](IntType_ptr type) -> Object_ptr
            {
                return call_native_method(call, ma, argument_types, "int");
            },
            [&](FloatType_ptr type) -> Object_ptr
            {
                return call_native_method(call, ma, argument_types, "float");
            },
            [&](StringType_ptr type) -> Object_ptr
            {
                return call_native_method(call, ma, argument_types, "str");
            },
            [&](ListType_ptr type) -> Object_ptr
            {
                return call_native_method(call, ma, argument_types, "list");
            },
            [&](TupleType_ptr type) -> Object_ptr
            {
                return call_native_method(call, ma, argument_types, "tuple");
            },
            [&](SetType_ptr type) -> Object_ptr
            {
                return call_native_method(call, ma, argument_types, "set");
            },
            [&](MapType_ptr type) -> Object_ptr
            {
                return call_native_method(call, ma, argument_types, "map");
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

            [&](IntersectionType_ptr inter_type) -> Object_ptr
            {
                ma.is_trait_dispatch = true;
                call.is_trait_dispatch = true;

                // Find the trait in the intersection that contains the method
                for (const auto& type : inter_type->types)
                {
                    Doctor::get().assert(
                        type->is<TraitType_ptr>(),
                        WaspStage::Semantics,
                        "Expected all types in an intersection to be traits "
                        "for method calls."
                    );

                    auto trait = type->as<TraitType_ptr>();

                    if (trait->contains_member(ma.right->as<Identifier>().name))
                    {
                        auto return_type = call_possible_method(
                            call,
                            ma,
                            argument_types,
                            trait
                        );

                        if (return_type)
                        {
                            call.trait_type_id = trait->type_id;
                            return return_type;
                        }
                    }
                }

                Doctor::get().fatal(
                    WaspStage::Semantics,
                    "None of the traits in the intersection contain method : " +
                        ma.right->as<Identifier>().name
                );
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

        if (!sig->template_type->exists())
        {
            index++;
        }
    }

    return index;
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

Object_ptr SemanticAnalyzer::call_possible_method(
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

    auto
        [best_signature_obj,
         overload_index] = type_system
                               ->get_possible_best_function_object(
                                   current_scope,
                                   signatures_set->types,
                                   argument_types
                               );

    if (overload_index == -1)
    {
        return nullptr;
    }

    access.member_index = oops_type->bag_type->get_index(method_name);

    call.overload_index = overload_index;
    call.is_method_call = true;

    return best_signature_obj->as<Signature_ptr>()->return_type;
}

Object_ptr SemanticAnalyzer::call_native_method(
    Call& call,
    MemberAccess& ma,
    const ObjectVector& argument_types,
    std::string native_class_name
)
{
    call.is_native_method_call = true;

    auto native_class = current_scope->lookup_required_and_resolve(
        native_class_name
    );

    auto native_class_type = native_class->get_type()->as<ClassType_ptr>();
    native_class_type->is_native = true;
    call.native_class_type_id = native_class_type->type_id;
    call.native_class_symbol_id = native_class->id;

    return call_method(call, ma, argument_types, native_class_type);
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
    if (expected_type->is<IntersectionType_ptr>())
    {
        auto intersection = expected_type->as<IntersectionType_ptr>();
        std::vector<int> trait_ids;

        for (const auto& trait_type : intersection->types)
        {
            Doctor::get().assert(
                trait_type->is<TraitType_ptr>(),
                WaspStage::Semantics,
                "Expected all types in an intersection to be traits for boxing."
            );

            Doctor::get().assert(
                actual_type->is<ClassType_ptr>(),
                WaspStage::Semantics,
                "Expected a class type to be boxed into a trait intersection."
            );

            auto trait = trait_type->as<TraitType_ptr>();
            trait_ids.push_back(trait->type_id);
        }

        return make_expression(Box(expr, trait_ids));
    }

    // Handle single trait
    if (expected_type->is<TraitType_ptr>() && actual_type->is<ClassType_ptr>())
    {
        if (type_system->assignable(current_scope, expected_type, actual_type))
        {
            auto trait = expected_type->as<TraitType_ptr>();
            return make_expression(Box(expr, {trait->type_id}));
        }
    }

    return expr;
}

} // namespace Wasp
