#include "Doctor.h"
#include "SymbolScope.h"
#include "Type.h"
#include "TypeSystem.h"
#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>

namespace Wasp
{

Type_ptr TypeSystem::unify(SymbolScope_ptr scope, const TypeVector& types)
{
    Doctor::semantics().check(!types.empty(), "Cannot unify an empty set of types");

    TypeVector unique_types = remove_duplicates(scope, types);

    if (unique_types.size() == 1)
    {
        return unique_types[0];
    }

    return make_type(std::make_shared<VariantType>(unique_types));
}

TypeVector TypeSystem::remove_duplicates(SymbolScope_ptr scope, const TypeVector& types)
{
    TypeVector unique_types;
    unique_types.reserve(types.size());

    for (const auto& item : types)
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

bool TypeSystem::infer_substitutions(
    SymbolScope_ptr scope,
    Type_ptr type,
    const Type_ptr& arg_type,
    TypeSubstitutionMap& substitutions
)
{
    if (!type || !arg_type)
    {
        return false;
    }

    if (type->is<GenericType_ptr>())
    {
        auto generic = type->as<GenericType_ptr>();
        const auto& name = generic->name;

        if (generic->constraint_type && !assignable(scope, generic->constraint_type, arg_type))
        {
            return false;
        }

        auto it = substitutions.find(name);
        if (it != substitutions.end())
        {
            return equal(scope, it->second, arg_type);
        }

        substitutions[name] = arg_type;
        return true;
    }

    if (type->is<ListType_ptr>() && arg_type->is<ListType_ptr>())
    {
        return infer_substitutions(
            scope,
            type->as<ListType_ptr>()->element_type,
            arg_type->as<ListType_ptr>()->element_type,
            substitutions
        );
    }

    if (type->is<SetType_ptr>() && arg_type->is<SetType_ptr>())
    {
        return infer_substitutions(
            scope,
            type->as<SetType_ptr>()->element_type,
            arg_type->as<SetType_ptr>()->element_type,
            substitutions
        );
    }

    if (type->is<MapType_ptr>() && arg_type->is<MapType_ptr>())
    {
        auto pt = type->as<MapType_ptr>();
        auto at = arg_type->as<MapType_ptr>();

        if (!infer_substitutions(scope, pt->key_type, at->key_type, substitutions))
        {
            return false;
        }

        return infer_substitutions(scope, pt->value_type, at->value_type, substitutions);
    }

    if (type->is<TupleType_ptr>() && arg_type->is<TupleType_ptr>())
    {
        auto pt = type->as<TupleType_ptr>();
        auto at = arg_type->as<TupleType_ptr>();

        if (pt->element_types.size() != at->element_types.size())
        {
            return false;
        }

        for (size_t i = 0; i < pt->element_types.size(); ++i)
        {
            if (!infer_substitutions(scope, pt->element_types[i], at->element_types[i], substitutions))
            {
                return false;
            }
        }

        return true;
    }

    if (type->is<VariantType_ptr>() && arg_type->is<VariantType_ptr>())
    {
        auto pt = type->as<VariantType_ptr>();
        auto at = arg_type->as<VariantType_ptr>();

        if (pt->types.size() != at->types.size())
        {
            return false;
        }

        for (size_t i = 0; i < pt->types.size(); ++i)
        {
            if (!infer_substitutions(scope, pt->types[i], at->types[i], substitutions))
            {
                return false;
            }
        }

        return true;
    }

    if (type->is<IntersectionType_ptr>() && arg_type->is<IntersectionType_ptr>())
    {
        auto pt = type->as<IntersectionType_ptr>();
        auto at = arg_type->as<IntersectionType_ptr>();

        if (pt->types.size() != at->types.size())
        {
            return false;
        }

        for (size_t i = 0; i < pt->types.size(); ++i)
        {
            if (!infer_substitutions(scope, pt->types[i], at->types[i], substitutions))
            {
                return false;
            }
        }

        return true;
    }

    return assignable(scope, type, arg_type);
}

OptionalTypeSubstitutionMap TypeSystem::infer_solid_types(
    SymbolScope_ptr scope,
    const TypeVector& formal_parameter_types,
    const StringVector& template_parameter_names,
    const TypeVector& actual_argument_types
)
{
    if (formal_parameter_types.size() != actual_argument_types.size())
    {
        return std::nullopt;
    }

    TypeSubstitutionMap substitutions;

    for (size_t i = 0; i < formal_parameter_types.size(); ++i)
    {
        bool ok = infer_substitutions(
            scope,
            formal_parameter_types[i],
            actual_argument_types[i],
            substitutions
        );

        if (!ok)
        {
            return std::nullopt;
        }
    }

    for (const auto& name : template_parameter_names)
    {
        if (substitutions.find(name) == substitutions.end())
        {
            return std::nullopt;
        }
    }

    return substitutions;
}

} // namespace Wasp
