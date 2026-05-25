#include "Doctor.h"
#include "Objects.h"
#include "SymbolScope.h"
#include "TypeSystem.h"

#include <algorithm>
#include <variant>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

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
        if (resolve_generics && type->is<TemplateParameterType_ptr>())
        {
            auto param = type->as<TemplateParameterType_ptr>();

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
            [&](ListType const& t)
            {
                return t.element_type;
            },
            [&](TupleType const& t)
            {
                return make_object(VariantType(t.element_types));
            },
            [&](MapType const& t)
            {
                return make_object(
                    TupleType(ObjectVector{t.key_type, t.value_type})
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

    return make_object(VariantType(unique_types));
}

} // namespace Wasp
