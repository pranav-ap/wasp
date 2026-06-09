#include "Doctor.h"
#include "Objects.h"
#include "Token.h"
#include "TypeSystem.h"
#include "Workspace.h"

#include <algorithm>
#include <cstddef>
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

Object_ptr TypeSystem::get_least_upper_bound(
    SymbolScope_ptr scope,
    ObjectVector types
) const
{
    Doctor::get().assert(
        !types.empty(),
        WaspStage::Semantics,
        "Cannot compute least upper bound of an empty set of types"
    );

    if (types.size() == 1)
    {
        return types[0];
    }

    Object_ptr unified_type = types[0];

    for (size_t i = 1; i < types.size(); ++i)
    {
        if (equal(scope, unified_type, types[i]))
        {
            continue;
        }

        unified_type = get_least_upper_bound(scope, unified_type, types[i]);
    }

    return unified_type;
}

Object_ptr TypeSystem::get_least_upper_bound(
    SymbolScope_ptr scope,
    Object_ptr a,
    Object_ptr b
) const
{
    // If they're equal, return either
    if (equal(scope, a, b))
    {
        return a;
    }

    // Check if one is a subtype of the other
    if (assignable(scope, b, a))
    {
        return a; // a is supertype of b
    }

    if (assignable(scope, a, b))
    {
        return b; // b is supertype of a
    }

    // Create a variant type containing just these two distinct types
    ObjectVector combined;

    if (a->is<VariantType_ptr>())
    {
        auto variant_a = a->as<VariantType_ptr>();
        for (const auto& t : variant_a->types)
        {
            combined.push_back(t);
        }
    }
    else
    {
        combined.push_back(a);
    }

    if (b->is<VariantType_ptr>())
    {
        auto variant_b = b->as<VariantType_ptr>();
        for (const auto& t : variant_b->types)
        {
            combined.push_back(t);
        }
    }
    else
    {
        combined.push_back(b);
    }

    combined = remove_duplicates(scope, combined);

    if (combined.size() == 1)
    {
        return combined[0];
    }

    return make_object(std::make_shared<VariantType>(combined));
}

// ============================================================================
// Equality Checks
// ============================================================================

bool TypeSystem::equal(
    SymbolScope_ptr scope,
    const Object_ptr type_1,
    const Object_ptr type_2
) const
{
    if (!type_1 || !type_2)
    {
        return false;
    }

    if (type_1 == type_2)
    {
        return true;
    }

    Object_ptr t1 = resolve_type(type_1, false);
    Object_ptr t2 = resolve_type(type_2, false);

    if (t1 == t2)
    {
        return true;
    }

    if (t1->is<VariantType_ptr>() && t2->is<VariantType_ptr>())
    {
        return equal_unordered(
            scope,
            t1->as<VariantType_ptr>()->types,
            t2->as<VariantType_ptr>()->types
        );
    }

    if (t1->is<IntersectionType_ptr>() && t2->is<IntersectionType_ptr>())
    {
        return equal_unordered(
            scope,
            t1->as<IntersectionType_ptr>()->types,
            t2->as<IntersectionType_ptr>()->types
        );
    }

    if (t1->is<GenericType_ptr>() && t2->is<GenericType_ptr>())
    {
        auto g1 = t1->as<GenericType_ptr>();
        auto g2 = t2->as<GenericType_ptr>();
        return equal(scope, g1->constraint_type, g2->constraint_type);
    }

    return Object::are_equal_types(t1, t2);
}

bool TypeSystem::equal(
    SymbolScope_ptr scope,
    const ObjectVector& type_vector_1,
    const ObjectVector& type_vector_2
) const
{
    if (type_vector_1.size() != type_vector_2.size())
    {
        return false;
    }

    return std::equal(
        type_vector_1.begin(),
        type_vector_1.end(),
        type_vector_2.begin(),
        [&](const auto& a, const auto& b)
        {
            return equal(scope, a, b);
        }
    );
}

bool TypeSystem::equal_unordered(
    SymbolScope_ptr scope,
    const ObjectVector& left_vector,
    const ObjectVector& right_vector
) const
{
    if (left_vector.size() != right_vector.size())
    {
        return false;
    }

    return std::all_of(
        left_vector.begin(),
        left_vector.end(),
        [&](const auto& left)
        {
            return std::any_of(
                right_vector.begin(),
                right_vector.end(),
                [&](const auto& right)
                {
                    return equal(scope, left, right);
                }
            );
        }
    );
}

// ============================================================================
// Assignability Checks (LHS <- RHS)
// ============================================================================

bool TypeSystem::assignable(
    SymbolScope_ptr scope,
    const Object_ptr lhs_type,
    const Object_ptr rhs_type
) const
{
    if (!lhs_type || !rhs_type)
    {
        return false;
    }

    Object_ptr lhs = resolve_type(lhs_type, false);
    Object_ptr rhs = resolve_type(rhs_type, false);

    if (equal(scope, lhs, rhs))
    {
        return true;
    }

    // Generics
    if (rhs->is<GenericType_ptr>())
    {
        return assignable(
            scope,
            lhs,
            rhs->as<GenericType_ptr>()->constraint_type
        );
    }
    if (lhs->is<GenericType_ptr>())
    {
        return assignable(
            scope,
            lhs->as<GenericType_ptr>()->constraint_type,
            rhs
        );
    }

    // Variants
    if (rhs->is<VariantType_ptr>())
    {
        auto rhs_var = rhs->as<VariantType_ptr>();
        return std::all_of(
            rhs_var->types.begin(),
            rhs_var->types.end(),
            [&](Object_ptr t)
            {
                return assignable(scope, lhs, t);
            }
        );
    }

    if (lhs->is<VariantType_ptr>())
    {
        auto lhs_var = lhs->as<VariantType_ptr>();
        return std::any_of(
            lhs_var->types.begin(),
            lhs_var->types.end(),
            [&](Object_ptr t)
            {
                return assignable(scope, t, rhs);
            }
        );
    }

    // Intersection Types
    if (lhs->is<IntersectionType_ptr>())
    {
        auto lhs_int = lhs->as<IntersectionType_ptr>();
        return std::all_of(
            lhs_int->types.begin(),
            lhs_int->types.end(),
            [&](Object_ptr t)
            {
                return assignable(scope, t, rhs);
            }
        );
    }

    if (rhs->is<IntersectionType_ptr>())
    {
        auto rhs_int = rhs->as<IntersectionType_ptr>();
        return std::any_of(
            rhs_int->types.begin(),
            rhs_int->types.end(),
            [&](Object_ptr t)
            {
                return assignable(scope, lhs, t);
            }
        );
    }

    // Composite Types
    if (lhs->is<ListType_ptr>() && rhs->is<ListType_ptr>())
    {
        auto lhs_list = lhs->as<ListType_ptr>();
        auto rhs_list = rhs->as<ListType_ptr>();
        return assignable(
            scope,
            lhs_list->element_type,
            rhs_list->element_type
        );
    }

    if (lhs->is<SetType_ptr>() && rhs->is<SetType_ptr>())
    {
        auto lhs_set = lhs->as<SetType_ptr>();
        auto rhs_set = rhs->as<SetType_ptr>();
        return assignable(scope, lhs_set->element_type, rhs_set->element_type);
    }

    if (lhs->is<TupleType_ptr>() && rhs->is<TupleType_ptr>())
    {
        auto lhs_tuple = lhs->as<TupleType_ptr>();
        auto rhs_tuple = rhs->as<TupleType_ptr>();

        if (lhs_tuple->element_types.size() != rhs_tuple->element_types.size())
        {
            return false;
        }

        for (size_t i = 0; i < lhs_tuple->element_types.size(); i++)
        {
            if (!assignable(
                    scope,
                    lhs_tuple->element_types[i],
                    rhs_tuple->element_types[i]
                ))
            {
                return false;
            }
        }
        return true;
    }

    if (lhs->is<MapType_ptr>() && rhs->is<MapType_ptr>())
    {
        auto lhs_map = lhs->as<MapType_ptr>();
        auto rhs_map = rhs->as<MapType_ptr>();

        return assignable(scope, lhs_map->key_type, rhs_map->key_type) &&
               assignable(scope, lhs_map->value_type, rhs_map->value_type);
    }

    return std::visit(
        ::overloaded{
            [](AnyType_ptr, const auto&)
            {
                return true;
            },

            [](LiteralType_ptr l, LiteralType_ptr r)
            {
                return false;
            },

            [&](TraitType_ptr lhs_trait, ClassType_ptr rhs_class)
            {
                int target_id = lhs_trait->type_id;

                for (const auto& trait_obj : rhs_class->traits)
                {
                    if (auto trait_def = trait_obj->as<TraitType_ptr>())
                    {
                        if (trait_def->type_id == target_id)
                        {
                            return true;
                        }
                    }
                }
                return false;
            },

            // Mixes

            // List type to list class
            [&](ClassType_ptr lhs_class, ListType_ptr rhs_list) -> bool
            {
                if (!lhs_class->is_primitive || lhs_class->name != "list")
                {
                    return false;
                }
                // Get the first template parameter from the class
                if (!lhs_class->template_type ||
                    lhs_class->template_type->ordered_parameter_names.empty())
                {
                    return false;
                }
                const auto& param_name = lhs_class->template_type
                                             ->ordered_parameter_names[0];
                auto it = lhs_class->template_type->template_parameters.find(
                    param_name
                );
                if (it == lhs_class->template_type->template_parameters.end())
                {
                    return false;
                }
                auto lhs_element_type = it->second;
                return assignable(
                    scope,
                    lhs_element_type,
                    rhs_list->element_type
                );
            },

            [&](ListType_ptr lhs_list, ClassType_ptr rhs_class) -> bool
            {
                if (!rhs_class->is_primitive || rhs_class->name != "list")
                {
                    return false;
                }
                if (!rhs_class->template_type ||
                    rhs_class->template_type->ordered_parameter_names.empty())
                {
                    return false;
                }
                const auto& param_name = rhs_class->template_type
                                             ->ordered_parameter_names[0];
                auto it = rhs_class->template_type->template_parameters.find(
                    param_name
                );
                if (it == rhs_class->template_type->template_parameters.end())
                {
                    return false;
                }
                auto rhs_element_type = it->second;
                return assignable(
                    scope,
                    lhs_list->element_type,
                    rhs_element_type
                );
            },

            [&](TupleType_ptr lhs_tuple, ClassType_ptr rhs_class) -> bool
            {
                if (!rhs_class->is_primitive || rhs_class->name != "tuple")
                {
                    return false;
                }
                if (!rhs_class->template_type ||
                    rhs_class->template_type->ordered_parameter_names.empty())
                {
                    return false;
                }
                const auto& param_name = rhs_class->template_type
                                             ->ordered_parameter_names[0];
                auto it = rhs_class->template_type->template_parameters.find(
                    param_name
                );
                if (it == rhs_class->template_type->template_parameters.end())
                {
                    return false;
                }
                auto rhs_element_types = it->second;

                // Resolve generic to its constraint
                if (rhs_element_types->is<GenericType_ptr>())
                {
                    rhs_element_types = rhs_element_types->as<GenericType_ptr>()
                                            ->constraint_type;
                }

                if (!rhs_element_types->is<TupleType_ptr>())
                {
                    return false;
                }

                auto rhs_tuple = rhs_element_types->as<TupleType_ptr>();

                if (lhs_tuple->element_types.size() !=
                    rhs_tuple->element_types.size())
                {
                    return false;
                }

                for (size_t i = 0; i < lhs_tuple->element_types.size(); i++)
                {
                    if (!assignable(
                            scope,
                            lhs_tuple->element_types[i],
                            rhs_tuple->element_types[i]
                        ))
                    {
                        return false;
                    }
                }
                return true;
            },

            [&](ClassType_ptr lhs_class, TupleType_ptr rhs_tuple) -> bool
            {
                if (!lhs_class->is_primitive || lhs_class->name != "tuple")
                {
                    return false;
                }
                if (lhs_class->template_type->ordered_parameter_names.empty())
                {
                    return false;
                }
                const auto& param_name = lhs_class->template_type
                                             ->ordered_parameter_names[0];
                auto it = lhs_class->template_type->template_parameters.find(
                    param_name
                );
                if (it == lhs_class->template_type->template_parameters.end())
                {
                    return false;
                }
                auto lhs_element_types = it->second;

                // Resolve generic to its constraint
                if (lhs_element_types->is<GenericType_ptr>())
                {
                    lhs_element_types = lhs_element_types->as<GenericType_ptr>()
                                            ->constraint_type;
                }

                if (!lhs_element_types->is<TupleType_ptr>())
                {
                    return false;
                }

                auto lhs_tuple = lhs_element_types->as<TupleType_ptr>();

                if (lhs_tuple->element_types.size() !=
                    rhs_tuple->element_types.size())
                {
                    return false;
                }

                for (size_t i = 0; i < lhs_tuple->element_types.size(); i++)
                {
                    if (!assignable(
                            scope,
                            lhs_tuple->element_types[i],
                            rhs_tuple->element_types[i]
                        ))
                    {
                        return false;
                    }
                }
                return true;
            },

            // Set type to set class
            [&](ClassType_ptr lhs_class, SetType_ptr rhs_set) -> bool
            {
                if (!lhs_class->is_primitive || lhs_class->name != "set")
                {
                    return false;
                }
                if (!lhs_class->template_type ||
                    lhs_class->template_type->ordered_parameter_names.empty())
                {
                    return false;
                }
                const auto& param_name = lhs_class->template_type
                                             ->ordered_parameter_names[0];
                auto it = lhs_class->template_type->template_parameters.find(
                    param_name
                );
                if (it == lhs_class->template_type->template_parameters.end())
                {
                    return false;
                }
                auto lhs_element_type = it->second;
                return assignable(
                    scope,
                    lhs_element_type,
                    rhs_set->element_type
                );
            },

            [&](SetType_ptr lhs_set, ClassType_ptr rhs_class) -> bool
            {
                if (!rhs_class->is_primitive || rhs_class->name != "set")
                {
                    return false;
                }
                if (!rhs_class->template_type ||
                    rhs_class->template_type->ordered_parameter_names.empty())
                {
                    return false;
                }
                const auto& param_name = rhs_class->template_type
                                             ->ordered_parameter_names[0];
                auto it = rhs_class->template_type->template_parameters.find(
                    param_name
                );
                if (it == rhs_class->template_type->template_parameters.end())
                {
                    return false;
                }
                auto rhs_element_type = it->second;
                return assignable(
                    scope,
                    lhs_set->element_type,
                    rhs_element_type
                );
            },

            // Map type to map class
            [&](ClassType_ptr lhs_class, MapType_ptr rhs_map) -> bool
            {
                if (!lhs_class->is_primitive || lhs_class->name != "map")
                {
                    return false;
                }
                if (!lhs_class->template_type ||
                    lhs_class->template_type->ordered_parameter_names.size() <
                        2)
                {
                    return false;
                }

                // Get key type
                const auto& key_param_name = lhs_class->template_type
                                                 ->ordered_parameter_names[0];
                auto key_it = lhs_class->template_type->template_parameters
                                  .find(key_param_name);
                if (key_it ==
                    lhs_class->template_type->template_parameters.end())
                {
                    return false;
                }

                // Get value type
                const auto& value_param_name = lhs_class->template_type
                                                   ->ordered_parameter_names[1];
                auto value_it = lhs_class->template_type->template_parameters
                                    .find(value_param_name);
                if (value_it ==
                    lhs_class->template_type->template_parameters.end())
                {
                    return false;
                }

                return assignable(scope, key_it->second, rhs_map->key_type) &&
                       assignable(scope, value_it->second, rhs_map->value_type);
            },

            [&](MapType_ptr lhs_map, ClassType_ptr rhs_class) -> bool
            {
                if (!rhs_class->is_primitive || rhs_class->name != "map")
                {
                    return false;
                }
                if (!rhs_class->template_type ||
                    rhs_class->template_type->ordered_parameter_names.size() <
                        2)
                {
                    return false;
                }

                // Get key type
                const auto& key_param_name = rhs_class->template_type
                                                 ->ordered_parameter_names[0];
                auto key_it = rhs_class->template_type->template_parameters
                                  .find(key_param_name);
                if (key_it ==
                    rhs_class->template_type->template_parameters.end())
                {
                    return false;
                }

                // Get value type
                const auto& value_param_name = rhs_class->template_type
                                                   ->ordered_parameter_names[1];
                auto value_it = rhs_class->template_type->template_parameters
                                    .find(value_param_name);
                if (value_it ==
                    rhs_class->template_type->template_parameters.end())
                {
                    return false;
                }

                return assignable(scope, lhs_map->key_type, key_it->second) &&
                       assignable(scope, lhs_map->value_type, value_it->second);
            },

            // Primitive type to native class
            [&](ClassType_ptr lhs_class, IntType_ptr) -> bool
            {
                return lhs_class->is_primitive && lhs_class->name == "int";
            },
            [&](IntType_ptr, ClassType_ptr rhs_class) -> bool
            {
                return rhs_class->is_primitive && rhs_class->name == "int";
            },

            [&](ClassType_ptr lhs_class, FloatType_ptr) -> bool
            {
                return lhs_class->is_primitive && lhs_class->name == "float";
            },
            [&](FloatType_ptr, ClassType_ptr rhs_class) -> bool
            {
                return rhs_class->is_primitive && rhs_class->name == "float";
            },

            [&](ClassType_ptr lhs_class, StringType_ptr) -> bool
            {
                return lhs_class->is_primitive && lhs_class->name == "str";
            },
            [&](StringType_ptr, ClassType_ptr rhs_class) -> bool
            {
                return rhs_class->is_primitive && rhs_class->name == "str";
            },

            [&](ClassType_ptr lhs_class, BooleanType_ptr) -> bool
            {
                return lhs_class->is_primitive && lhs_class->name == "bool";
            },
            [&](BooleanType_ptr, ClassType_ptr rhs_class) -> bool
            {
                return rhs_class->is_primitive && rhs_class->name == "bool";
            },

            [](const auto&, const auto&)
            {
                return false;
            }
        },
        lhs->value,
        rhs->value
    );
}

bool TypeSystem::assignable(
    SymbolScope_ptr scope,
    const ObjectVector& type_vector_1,
    const ObjectVector& type_vector_2
) const
{
    if (type_vector_1.size() != type_vector_2.size())
    {
        return false;
    }

    return std::equal(
        type_vector_1.begin(),
        type_vector_1.end(),
        type_vector_2.begin(),
        [&](const auto& a, const auto& b)
        {
            return assignable(scope, a, b);
        }
    );
}

// ============================================================================
// Inference
// ============================================================================

Object_ptr TypeSystem::infer(
    SymbolScope_ptr scope,
    Object_ptr left_type,
    TokenType op,
    Object_ptr right_type
)
{
    left_type = resolve_type(left_type, true);
    right_type = resolve_type(right_type, true);

    if (left_type->is<VariantType_ptr>())
    {
        ObjectVector result_types;
        auto variant = left_type->as<VariantType_ptr>();
        for (const auto& t : variant->types)
        {
            result_types.push_back(infer(scope, t, op, right_type));
        }
        return make_object(std::make_shared<VariantType>(result_types));
    }

    if (left_type->is<IntersectionType_ptr>())
    {
        ObjectVector result_types;
        auto intersection = left_type->as<IntersectionType_ptr>();
        for (const auto& t : intersection->types)
        {
            result_types.push_back(infer(scope, t, op, right_type));
        }
        return make_object(std::make_shared<IntersectionType>(result_types));
    }

    if (right_type->is<VariantType_ptr>())
    {
        ObjectVector result_types;
        auto variant = right_type->as<VariantType_ptr>();
        for (const auto& t : variant->types)
        {
            result_types.push_back(infer(scope, left_type, op, t));
        }
        return make_object(std::make_shared<VariantType>(result_types));
    }

    if (right_type->is<IntersectionType_ptr>())
    {
        ObjectVector result_types;
        auto intersection = right_type->as<IntersectionType_ptr>();
        for (const auto& t : intersection->types)
        {
            result_types.push_back(infer(scope, left_type, op, t));
        }
        return make_object(std::make_shared<IntersectionType>(result_types));
    }

    switch (op)
    {
    case TokenType::PLUS:
        if (is_string_type(left_type) || is_string_type(right_type))
        {
            bool left_valid = is_string_type(left_type) ||
                              is_number_type(left_type);
            bool right_valid = is_string_type(right_type) ||
                               is_number_type(right_type);
            if (!left_valid)
            {
                Doctor::get().fatal(
                    WaspStage::Semantics,
                    "Invalid concatenation: Left is '" +
                        left_type->to_string() + "'"
                );
            }
            if (!right_valid)
            {
                Doctor::get().fatal(
                    WaspStage::Semantics,
                    "Invalid concatenation: Right is '" +
                        right_type->to_string() + "'"
                );
            }
            return pool->get_string_type();
        }
        [[fallthrough]];
    case TokenType::STAR:
    case TokenType::POWER:
    case TokenType::MINUS:
    case TokenType::DIVISION:
    case TokenType::MOD:
        Doctor::get().assert(
            is_number_type(left_type),
            WaspStage::Semantics,
            "Left operand must be a number, got '" + left_type->to_string() +
                "'"
        );

        Doctor::get().assert(
            is_number_type(right_type),
            WaspStage::Semantics,
            "Right operand must be a number, got '" + right_type->to_string() +
                "'"
        );

        return (is_float_type(left_type) || is_float_type(right_type))
                   ? pool->get_float_type()
                   : pool->get_int_type();

    case TokenType::LESSER_THAN:
    case TokenType::LESSER_THAN_EQUAL:
    case TokenType::GREATER_THAN:
    case TokenType::GREATER_THAN_EQUAL:
        Doctor::get().assert(
            is_number_type(left_type),
            WaspStage::Semantics,
            "Left operand must be a number, got '" + left_type->to_string() +
                "'"
        );

        Doctor::get().assert(
            is_number_type(right_type),
            WaspStage::Semantics,
            "Right operand must be a number, got '" + right_type->to_string() +
                "'"
        );

        return pool->get_boolean_type();

    case TokenType::EQUAL_EQUAL:
    case TokenType::BANG_EQUAL:
        if (left_type->is<NoneType_ptr>() || right_type->is<NoneType_ptr>())
        {
            return pool->get_boolean_type();
        }
        if (is_number_type(left_type))
        {
            Doctor::get().assert(
                is_number_type(right_type),
                WaspStage::Semantics,
                "Right operand must be a number, got '" +
                    right_type->to_string() + "'"
            );
        }
        else if (is_string_type(left_type))
        {
            Doctor::get().assert(
                is_string_type(right_type),
                WaspStage::Semantics,
                "Right operand must be a string, got '" +
                    right_type->to_string() + "'"
            );
        }
        else if (is_boolean_type(left_type))
        {
            Doctor::get().assert(
                is_boolean_type(right_type),
                WaspStage::Semantics,
                "Right operand must be a boolean, got '" +
                    right_type->to_string() + "'"
            );
        }
        else if (left_type->is<EnumType_ptr>())
        {
            Doctor::get().assert(
                equal(scope, left_type, right_type),
                WaspStage::Semantics,
                "Enum mismatch"
            );
        }
        else
        {
            Doctor::get().fatal(
                WaspStage::Semantics,
                "Cannot compare '" + left_type->to_string() + "' with '" +
                    right_type->to_string() + "'"
            );
        }
        return pool->get_boolean_type();

    case TokenType::AND:
    case TokenType::OR:
        Doctor::get().assert(
            is_boolean_type(left_type),
            WaspStage::Semantics,
            "Left operand must be a boolean, got '" + left_type->to_string() +
                "'"
        );

        Doctor::get().assert(
            is_boolean_type(right_type),
            WaspStage::Semantics,
            "Right operand must be a boolean, got '" + right_type->to_string() +
                "'"
        );

        return pool->get_boolean_type();

    default:
        Doctor::get().fatal(
            WaspStage::Semantics,
            "Unsupported binary operator"
        );
    }

    return pool->get_none_type();
}

Object_ptr TypeSystem::infer(
    SymbolScope_ptr scope,
    Object_ptr operand_type,
    TokenType op
)
{
    operand_type = resolve_type(operand_type, true);

    if (operand_type->is<VariantType_ptr>())
    {
        ObjectVector result_types;
        auto variant = operand_type->as<VariantType_ptr>();
        for (const auto& t : variant->types)
        {
            result_types.push_back(infer(scope, t, op));
        }
        return make_object(std::make_shared<VariantType>(result_types));
    }

    if (operand_type->is<IntersectionType_ptr>())
    {
        ObjectVector result_types;
        auto intersection = operand_type->as<IntersectionType_ptr>();
        for (const auto& t : intersection->types)
        {
            result_types.push_back(infer(scope, t, op));
        }
        return make_object(std::make_shared<IntersectionType>(result_types));
    }

    if (operand_type->is<ClassType_ptr>())
    {
        auto class_type = operand_type->as<ClassType_ptr>();
        if (op == TokenType::DOT)
        {
            // Handle member access
        }
    }

    return pool->get_none_type();
}

// ============================================================================
// Trait
// =============================================================================

bool TypeSystem::implements_trait(
    SymbolScope_ptr scope,
    Object_ptr candidate_type,
    const std::string& trait_name
)
{
    candidate_type = candidate_type->unwrap_completely();

    if (!candidate_type->is<ClassType_ptr>() &&
        !candidate_type->is<TraitType_ptr>())
    {
        return false;
    }

    OopsType_ptr oop_type = candidate_type->is<ClassType_ptr>()
                                ? std::static_pointer_cast<OopsType>(
                                      candidate_type->as<ClassType_ptr>()
                                  )
                                : std::static_pointer_cast<OopsType>(
                                      candidate_type->as<TraitType_ptr>()
                                  );

    for (const auto& trait_obj : oop_type->traits)
    {
        if (trait_obj->is<TraitType_ptr>())
        {
            if (trait_obj->as<TraitType_ptr>()->name == trait_name)
            {
                return true;
            }
        }
    }

    return false;
}

} // namespace Wasp
