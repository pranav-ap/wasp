#include "SymbolScope.h"
#include "Type.h"
#include "TypeSystem.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <variant>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

bool TypeSystem::equal(
    SymbolScope_ptr scope,
    const TypeVector& type_vector_1,
    const TypeVector& type_vector_2
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
    const TypeVector& left_vector,
    const TypeVector& right_vector
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

bool TypeSystem::equal(
    SymbolScope_ptr scope,
    const Type_ptr type_1,
    const Type_ptr type_2
) const
{
    if (!type_1 || !type_2)
    {
        return false;
    }

    Type_ptr t1 = type_1->unwrap_alias();
    Type_ptr t2 = type_2->unwrap_alias();

    if (t1 == t2)
    {
        return true;
    }

    return std::visit(
        overloaded{
            [&](GenericType_ptr g1, GenericType_ptr g2)
            {
                return equal(
                    scope,
                    g1->constraint_type,
                    g2->constraint_type
                );
            },
            [&](VariantType_ptr l, VariantType_ptr r)
            {
                return equal_unordered(scope, l->types, r->types);
            },

            [&](IntersectionType_ptr l, IntersectionType_ptr r)
            {
                return equal_unordered(scope, l->types, r->types);
            },

            [&](Signature_ptr l, Signature_ptr r)
            {
                bool lhs_result = equal(
                    scope,
                    l->parameter_types,
                    r->parameter_types
                );

                bool rhs_result = equal(
                    scope,
                    l->return_type,
                    r->return_type
                );

                return lhs_result && rhs_result;
            },

            [](ClassType_ptr l, ClassType_ptr r)
            {
                return l->type_id == r->type_id;
            },
            [](TraitType_ptr l, TraitType_ptr r)
            {
                return l->type_id == r->type_id;
            },

            [](EnumType_ptr l, EnumType_ptr r) -> bool
            {
                auto get_root = [](const std::string& name)
                {
                    size_t pos = name.find('.');
                    return pos == std::string::npos ? name
                                                    : name.substr(0, pos);
                };

                return get_root(l->name) == get_root(r->name);
            },

            [&](TypeAlias_ptr l, TypeAlias_ptr r)
            {
                return equal(
                    scope,
                    l->underlying_type,
                    r->underlying_type
                );
            },

            // Catch-all identical Primitive Types
            []<typename T>(const T&, const T&)
            {
                return true;
            },

            // Default - Mismatch
            [](const auto&, const auto&)
            {
                return false;
            }
        },
        t1->data,
        t2->data
    );
}

} // namespace Wasp
