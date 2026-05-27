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
        // a is supertype of b
        return a;
    }

    if (assignable(scope, a, b))
    {
        // b is supertype of a
        return b;
    }

    // Create a variant type containing just these two distinct types

    ObjectVector combined;

    if (a->is<VariantType>())
    {
        auto variant_a = a->as<VariantType>();

        for (auto& t : variant_a.types)
        {
            combined.push_back(t);
        }
    }
    else
    {
        combined.push_back(a);
    }

    if (b->is<VariantType>())
    {
        auto variant_b = b->as<VariantType>();

        for (auto& t : variant_b.types)
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

    return make_object(VariantType{combined});
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

    if (t1->is<VariantType>() && t2->is<VariantType>())
    {
        return equal_unordered(
            scope,
            t1->as<VariantType>().types,
            t2->as<VariantType>().types
        );
    }

    if (t1->is<IntersectionType>() && t2->is<IntersectionType>())
    {
        return equal_unordered(
            scope,
            t1->as<IntersectionType>().types,
            t2->as<IntersectionType>().types
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
    if (rhs->is<VariantType>())
    {
        auto rhs_var = rhs->as<VariantType>();
        return std::all_of(
            rhs_var.types.begin(),
            rhs_var.types.end(),
            [&](Object_ptr t)
            {
                return assignable(scope, lhs, t);
            }
        );
    }

    if (lhs->is<VariantType>())
    {
        auto lhs_var = lhs->as<VariantType>();
        return std::any_of(
            lhs_var.types.begin(),
            lhs_var.types.end(),
            [&](Object_ptr t)
            {
                return assignable(scope, t, rhs);
            }
        );
    }

    // Intersection Types

    if (lhs->is<IntersectionType>())
    {
        auto lhs_int = lhs->as<IntersectionType>();
        return std::all_of(
            lhs_int.types.begin(),
            lhs_int.types.end(),
            [&](Object_ptr t)
            {
                return assignable(scope, t, rhs);
            }
        );
    }

    if (rhs->is<IntersectionType>())
    {
        auto rhs_int = rhs->as<IntersectionType>();
        return std::any_of(
            rhs_int.types.begin(),
            rhs_int.types.end(),
            [&](Object_ptr t)
            {
                return assignable(scope, lhs, t);
            }
        );
    }

    return std::visit(
        ::overloaded{
            [](AnyType const&, const auto&)
            {
                return true;
            },

            [](LiteralType const& l, LiteralType const& r)
            {
                return false;
            },

            [&](TraitType_ptr const lhs_trait, ClassType_ptr const rhs_class)
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

    if (left_type->is<VariantType>())
    {
        ObjectVector result_types;

        for (const auto& t : left_type->as<VariantType>().types)
        {
            result_types.push_back(infer(scope, t, op, right_type));
        }

        return std::make_shared<Object>(VariantType{result_types});
    }

    if (left_type->is<IntersectionType>())
    {
        ObjectVector result_types;

        for (const auto& t : left_type->as<IntersectionType>().types)
        {
            result_types.push_back(infer(scope, t, op, right_type));
        }

        return std::make_shared<Object>(IntersectionType{result_types});
    }

    if (right_type->is<VariantType>())
    {
        ObjectVector result_types;
        for (const auto& t : right_type->as<VariantType>().types)
        {
            result_types.push_back(infer(scope, left_type, op, t));
        }
        return std::make_shared<Object>(VariantType{result_types});
    }

    if (right_type->is<IntersectionType>())
    {
        ObjectVector result_types;
        for (const auto& t : right_type->as<IntersectionType>().types)
        {
            result_types.push_back(infer(scope, left_type, op, t));
        }
        return std::make_shared<Object>(IntersectionType{result_types});
    }

    switch (op)
    {
    case TokenType::EQUAL_EQUAL:
    case TokenType::BANG_EQUAL:
        if (left_type->is<EnumMemberType_ptr>() &&
            right_type->is<EnumMemberType_ptr>())
        {
            auto left_enum = left_type->as<EnumMemberType_ptr>();
            auto right_enum = right_type->as<EnumMemberType_ptr>();

            Doctor::get().assert(
                left_enum->enum_type->type_id == right_enum->enum_type->type_id,
                WaspStage::Semantics,
                "Cannot compare enum members of different types."
            );
        }
        return pool->get_boolean_type();

    default:
        Doctor::get().fatal(WaspStage::Semantics, "Unsupported binary operator");
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

    if (operand_type->is<VariantType>())
    {
        ObjectVector result_types;
        for (const auto& t : operand_type->as<VariantType>().types)
        {
            result_types.push_back(infer(scope, t, op));
        }
        return std::make_shared<Object>(VariantType{result_types});
    }

    if (operand_type->is<IntersectionType>())
    {
        ObjectVector result_types;
        for (const auto& t : operand_type->as<IntersectionType>().types)
        {
            result_types.push_back(infer(scope, t, op));
        }
        return std::make_shared<Object>(IntersectionType{result_types});
    }

    if (operand_type->is<ClassType_ptr>())
    {
        auto class_type = operand_type->as<ClassType_ptr>();

        if (op == TokenType::DOT)
        {
        }
    }

    return pool->get_none_type();
}

} // namespace Wasp
