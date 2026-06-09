#include "Doctor.h"
#include "Objects.h"
#include "TypeSystem.h"
#include "Workspace.h"

#include <algorithm>
#include <memory>
#include <variant>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

// =========================================================================
// Utilities
// =========================================================================

Object_ptr TypeSystem::resolve_type(
    Object_ptr type,
    bool resolve_generics
) const
{
    if (!type)
    {
        return nullptr;
    }

    ObjectVector visited_type_aliases;

    while (true)
    {
        // Resolve Aliases
        if (type->is<TypeAlias_ptr>())
        {
            auto alias = type->as<TypeAlias_ptr>();

            Doctor::get().assert(
                std::find(
                    visited_type_aliases.begin(),
                    visited_type_aliases.end(),
                    type
                ) == visited_type_aliases.end(),
                WaspStage::Semantics,
                "Circular type alias detected: " + alias->name
            );

            visited_type_aliases.push_back(type);

            type = alias->underlying_type;
            continue;
        }

        // Resolve Template Parameters
        if (resolve_generics && type->is<GenericType_ptr>())
        {
            auto param = type->as<GenericType_ptr>();

            Doctor::get().fatal_if_nullptr(
                param->constraint_type,
                WaspStage::Semantics,
                "Unconstrained generic type '" + param->name +
                    "' cannot be resolved"
            );

            type = param->constraint_type;
        }

        // If we hit a non-alias/non-generic, we are done
        break;
    }

    return type;
}

Object_ptr TypeSystem::spread_type(Object_ptr type)
{
    Doctor::get().fatal_if_nullptr(type, WaspStage::Semantics);

    return std::visit(
        ::overloaded{
            [&](ListType_ptr t)
            {
                return t->element_type;
            },
            [&](TupleType_ptr t)
            {
                return make_object(
                    std::make_shared<VariantType>(t->element_types)
                );
            },
            [&](MapType_ptr t)
            {
                return make_object(
                    std::make_shared<TupleType>(
                        ObjectVector{t->key_type, t->value_type}
                    )
                );
            },
            [&](const auto&) -> Object_ptr
            {
                Doctor::get().fatal(
                    WaspStage::Semantics,
                    "Cannot spread a non-iterable type"
                );
            }
        },
        type->value
    );
}

ObjectVector TypeSystem::remove_duplicates(
    SymbolScope_ptr scope,
    const ObjectVector& vec
) const
{
    ObjectVector unique_types;
    unique_types.reserve(vec.size());

    for (const auto& item : vec)
    {
        bool is_any_equal = std::any_of(
            unique_types.begin(),
            unique_types.end(),
            [&](const auto& i)
            {
                return equal(scope, i, item);
            }
        );

        if (!is_any_equal)
        {
            unique_types.push_back(item);
        }
    }

    return unique_types;
}

Object_ptr TypeSystem::unify(SymbolScope_ptr scope, const ObjectVector& types)
{
    Doctor::get().assert(
        !types.empty(),
        WaspStage::Semantics,
        "Cannot unify an empty set of types"
    );

    ObjectVector unique_types = remove_duplicates(scope, types);

    if (unique_types.size() == 1)
    {
        return unique_types[0];
    }

    return make_object(std::make_shared<VariantType>(unique_types));
}

// =========================================================================
// Type Checks
// =========================================================================

bool TypeSystem::is_int_type(Object_ptr obj) const
{
    Doctor::get().fatal_if_nullptr(obj, WaspStage::Semantics);

    if (obj->is<IntType_ptr>())
    {
        return true;
    }

    if (obj->is<LiteralType_ptr>())
    {
        auto lit = obj->as<LiteralType_ptr>();
        return lit->value->is<IntObject_ptr>();
    }

    return false;
}

bool TypeSystem::is_float_type(Object_ptr obj) const
{
    Doctor::get().fatal_if_nullptr(obj, WaspStage::Semantics);

    if (obj->is<FloatType_ptr>())
    {
        return true;
    }

    if (obj->is<LiteralType_ptr>())
    {
        auto lit = obj->as<LiteralType_ptr>();
        return lit->value->is<FloatObject_ptr>();
    }

    return false;
}

bool TypeSystem::is_number_type(Object_ptr obj) const
{
    return is_int_type(obj) || is_float_type(obj);
}

bool TypeSystem::is_string_type(Object_ptr obj) const
{
    Doctor::get().fatal_if_nullptr(obj, WaspStage::Semantics);

    if (obj->is<StringType_ptr>())
    {
        return true;
    }

    if (obj->is<LiteralType_ptr>())
    {
        auto lit = obj->as<LiteralType_ptr>();
        return lit->value->is<StringObject_ptr>();
    }

    return false;
}

bool TypeSystem::is_boolean_type(Object_ptr obj) const
{
    Doctor::get().fatal_if_nullptr(obj, WaspStage::Semantics);

    if (obj->is<BooleanType_ptr>())
    {
        return true;
    }

    if (obj->is<LiteralType_ptr>())
    {
        auto lit = obj->as<LiteralType_ptr>();
        return lit->value->is<BooleanObject_ptr>();
    }

    return false;
}

bool TypeSystem::is_none_type(const Object_ptr type) const
{
    Doctor::get().fatal_if_nullptr(type, WaspStage::Semantics);
    return type->is<NoneType_ptr>();
}

bool TypeSystem::is_native_type(const Object_ptr type) const
{
    Doctor::get().fatal_if_nullptr(type, WaspStage::Semantics);

    return type->is<IntType_ptr>() || type->is<FloatType_ptr>() ||
           type->is<StringType_ptr>() || type->is<BooleanType_ptr>() ||
           type->is<NoneType_ptr>() || type->is<AnyType_ptr>() ||
           type->is<LiteralType_ptr>() || type->is<ListType_ptr>() ||
           type->is<SetType_ptr>() || type->is<MapType_ptr>() ||
           type->is<TupleType_ptr>() || type->is<VariantType_ptr>() ||
           type->is<IntersectionType_ptr>();
}

bool TypeSystem::is_condition_type(
    SymbolScope_ptr scope,
    const Object_ptr condition_type
) const
{
    Doctor::get().fatal_if_nullptr(condition_type, WaspStage::Semantics);

    Object_ptr t = condition_type;

    if (t->is<TypeAlias_ptr>())
    {
        t = t->as<TypeAlias_ptr>()->underlying_type;
    }

    return std::visit(
        ::overloaded{
            [](BooleanType_ptr)
            {
                return true;
            },
            [](StringType_ptr)
            {
                return true;
            },
            [](ListType_ptr)
            {
                return true;
            },
            [](TupleType_ptr)
            {
                return true;
            },
            [](SetType_ptr)
            {
                return true;
            },
            [](MapType_ptr)
            {
                return true;
            },

            [](LiteralType_ptr lit)
            {
                return lit->value->is<BooleanObject_ptr>() ||
                       lit->value->is<StringObject_ptr>();
            },

            [&](VariantType_ptr v)
            {
                // A variant is only a valid condition if ALL its
                // possible types are truthy-compatible.
                return std::all_of(
                    v->types.begin(),
                    v->types.end(),
                    [&](Object_ptr o)
                    {
                        return is_condition_type(scope, o);
                    }
                );
            },

            [&](IntersectionType_ptr t)
            {
                // An intersection is a valid condition if ANY of its
                // possible types are truthy-compatible.
                return std::any_of(
                    t->types.begin(),
                    t->types.end(),
                    [&](Object_ptr o)
                    {
                        return is_condition_type(scope, o);
                    }
                );
            },

            [](const auto&)
            {
                return false;
            }
        },
        t->value
    );
}

bool TypeSystem::is_spreadable_type(
    SymbolScope_ptr scope,
    const Object_ptr candidate_type
) const
{
    Doctor::get().fatal_if_nullptr(candidate_type, WaspStage::Semantics);

    return std::visit(
        ::overloaded{
            [&](ListType_ptr)
            {
                return true;
            },
            [&](TupleType_ptr)
            {
                return true;
            },
            [&](MapType_ptr)
            {
                return true;
            },
            [&](VariantType_ptr t)
            {
                return std::all_of(
                    t->types.begin(),
                    t->types.end(),
                    [&](Object_ptr o)
                    {
                        return is_spreadable_type(scope, o);
                    }
                );
            },
            [&](IntersectionType_ptr t)
            {
                return std::any_of(
                    t->types.begin(),
                    t->types.end(),
                    [&](Object_ptr o)
                    {
                        return is_spreadable_type(scope, o);
                    }
                );
            },
            [](const auto&)
            {
                return false;
            }
        },
        candidate_type->value
    );
}

bool TypeSystem::is_iterable_type(
    SymbolScope_ptr scope,
    const Object_ptr candidate_type
) const
{
    return is_string_type(candidate_type) ||
           is_spreadable_type(scope, candidate_type);
}

bool TypeSystem::is_key_type(
    SymbolScope_ptr scope,
    const Object_ptr key_type
) const
{
    return is_int_type(key_type) || is_string_type(key_type) ||
           is_boolean_type(key_type);
}

Object_ptr TypeSystem::extract_iterable_element_type(
    SymbolScope_ptr scope,
    const Object_ptr type
) const
{
    Doctor::get().fatal_if_nullptr(type, WaspStage::Semantics);

    if (type->is<VariantType_ptr>())
    {
        auto variant = type->as<VariantType_ptr>();
        ObjectVector extracted_elements;
        for (const auto& t : variant->types)
        {
            extracted_elements.push_back(
                extract_iterable_element_type(scope, t)
            );
        }

        ObjectVector unique_elements = remove_duplicates(
            scope,
            extracted_elements
        );

        return unique_elements.size() == 1
                   ? unique_elements[0]
                   : make_object(
                         std::make_shared<VariantType>(unique_elements)
                     );
    }

    return std::visit(
        ::overloaded{
            [](ListType_ptr t) -> Object_ptr
            {
                return t->element_type;
            },
            [](SetType_ptr t) -> Object_ptr
            {
                return t->element_type;
            },
            [](MapType_ptr t) -> Object_ptr
            {
                return make_object(
                    std::make_shared<TupleType>(
                        ObjectVector{t->key_type, t->value_type}
                    )
                );
            },
            [&](TupleType_ptr t) -> Object_ptr
            {
                ObjectVector unique = remove_duplicates(
                    scope,
                    t->element_types
                );
                return unique.size() == 1
                           ? unique[0]
                           : make_object(std::make_shared<VariantType>(unique));
            },
            [&](StringType_ptr) -> Object_ptr
            {
                return pool->get_string_type();
            },
            [&](const auto&) -> Object_ptr
            {
                return pool->get_any_type();
            }
        },
        type->value
    );
}

} // namespace Wasp
