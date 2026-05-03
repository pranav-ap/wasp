#include "Doctor.h"
#include "Objects.h"
#include "TypeChecker.h"

#include "SymbolScope.h"
#include "Workspace.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

SymbolVector::iterator TypeChecker::find_matching_signature(
    SymbolScope_ptr scope,
    SymbolVector& target_vector,
    const ObjectVector& parameter_types
)
{
    return std::find_if(
        target_vector.begin(),
        target_vector.end(),
        [&](const Symbol_ptr& sym)
        {
            auto existing_signature = get_function_signature(sym->get_type());

            // Skip unresolved functions or non-functions
            if (!existing_signature)
                return false;

            if (existing_signature->parameter_types.size() != parameter_types.size())
                return false;

            for (size_t i = 0; i < parameter_types.size(); ++i)
            {
                if (!equal(scope, existing_signature->parameter_types[i], parameter_types[i]))
                    return false;
            }

            return true;
        }
    );
}

void TypeChecker::validate_new_function_overload(
    SymbolScope_ptr scope,
    std::string& function_name,
    const Symbol_ptr new_function_symbol
)
{
    auto existing_symbol = scope->lookup(function_name);

    if (!existing_symbol)
    {
        return;
    }

    auto new_type = new_function_symbol->get_type();
    auto new_function_signature = get_function_signature(new_type);

    Doctor::get().fatal_if_nullptr(
        new_function_signature,
        WaspStage::Semantics,
        "New function symbol lacks a valid function type"
    );

    const auto& parameter_types = new_function_signature->parameter_types;

    bool new_is_template = false;
    size_t new_generic_count = 0;

    if (auto t = new_type->try_as<std::shared_ptr<FunctionTemplateType>>())
    {
        new_is_template = true;
        new_generic_count = (*t)->generics.size();
    }

    // Helper to check against a single existing symbol
    auto check_duplicate = [&](const Symbol_ptr& sibling)
    {
        if (sibling == new_function_symbol)
            return;

        auto type_obj = sibling->get_type();
        if (!type_obj)
            return;

        auto existing_signature = get_function_signature(type_obj);

        Doctor::get().assert(
            existing_signature != nullptr,
            WaspStage::Semantics,
            "Existing symbol does not contain a valid function signature"
        );

        bool existing_is_template = false;
        size_t existing_generic_count = 0;

        if (auto t = type_obj->try_as<std::shared_ptr<FunctionTemplateType>>())
        {
            existing_is_template = true;
            existing_generic_count = (*t)->generics.size();
        }

        bool signatures_match = equal(scope, existing_signature->parameter_types, parameter_types);
        bool templates_match = (new_is_template == existing_is_template) &&
                               (new_generic_count == existing_generic_count);

        Doctor::get().assert(
            !(signatures_match && templates_match),
            WaspStage::Semantics,
            "Duplicate function signatures for '" + function_name + "' defined in same scope"
        );
    };

    if (existing_symbol->payload_is<FunctionOverloadsData>())
    {
        auto& group_data = existing_symbol->get_payload_as<FunctionOverloadsData>();

        // Sibling Duplicate Check
        for (const auto& sibling : group_data.siblings)
        {
            check_duplicate(sibling);
        }

        // Shadow Parents
        auto parent_match = std::find_if(
            group_data.parents.begin(),
            group_data.parents.end(),
            [&](const Symbol_ptr& parent)
            {
                auto parent_type = parent->get_type();
                if (!parent_type)
                    return false;

                auto parent_signature = get_function_signature(parent_type);
                if (!parent_signature)
                    return false;

                bool parent_is_template = false;
                size_t parent_generic_count = 0;

                if (auto t = parent_type->try_as<std::shared_ptr<FunctionTemplateType>>())
                {
                    parent_is_template = true;
                    parent_generic_count = (*t)->generics.size();
                }

                return (parent_is_template == new_is_template) &&
                       (parent_generic_count == new_generic_count) &&
                       equal(scope, parent_signature->parameter_types, parameter_types);
            }
        );

        if (parent_match != group_data.parents.end())
        {
            group_data.parents.erase(parent_match);
        }
    }
    else if (
        existing_symbol->payload_is<TemplateData>() || existing_symbol->payload_is<FunctionData>()
    )
    {
        // If it's a raw symbol that hasn't been grouped yet, just check against it directly
        check_duplicate(existing_symbol);
    }
    else
    {
        Doctor::get().fatal(
            WaspStage::Semantics,
            "Symbol '" + function_name + "' is not a function or template and cannot be overloaded."
        );
    }
}

void TypeChecker::validate_new_method_overload(
    SymbolScope_ptr scope,
    ObjectVector existing_overloads,
    const Symbol_ptr new_method_symbol
)
{
    auto new_method_signature = get_function_signature(new_method_symbol->get_type());

    Doctor::get().fatal_if_nullptr(
        new_method_signature,
        WaspStage::Semantics,
        "New method symbol lacks a valid function type"
    );

    const auto& parameter_types = new_method_signature->parameter_types;

    for (const auto& existing_overload : existing_overloads)
    {
        auto existing_signature = get_function_signature(existing_overload);

        Doctor::get().assert(
            existing_signature != nullptr,
            WaspStage::Semantics,
            "Overload group contains a non-function symbol"
        );

        if (equal(scope, existing_signature->parameter_types, parameter_types))
        {
            Doctor::get().fatal(
                WaspStage::Semantics,
                "Duplicate method signatures for '" + new_method_symbol->name +
                    "' defined in same class"
            );
        }
    }
}

std::tuple<Symbol_ptr, int> TypeChecker::get_best_function_signature(
    SymbolScope_ptr scope,
    const SymbolVector& candidates,
    const ObjectVector& argument_types
) const
{
    SymbolVector valid_matches;
    std::vector<int> match_indices;

    for (size_t i = 0; i < candidates.size(); ++i)
    {
        auto signature = get_function_signature(candidates[i]->get_type());

        if (!signature)
            continue;

        if (assignable(scope, signature->parameter_types, argument_types))
        {
            valid_matches.push_back(candidates[i]);
            match_indices.push_back(static_cast<int>(i));
        }
    }

    Doctor::get().assert(
        !valid_matches.empty(),
        WaspStage::Semantics,
        "No matching function signature found"
    );

    Doctor::get()
        .assert(valid_matches.size() == 1, WaspStage::Semantics, "Ambiguous function call");

    return {valid_matches.front(), match_indices.front()};
}

Object_ptr TypeChecker::substitute_generics(
    Object_ptr type,
    TemplateType_ptr templ,
    const ObjectVector& generic_args
) const
{
    if (!type)
        return nullptr;

    if (type->is<TypeAlias_ptr>())
    {
        auto aliased = type->as<TypeAlias_ptr>()->aliased_type;
        auto substituted = substitute_generics(aliased, templ, generic_args);

        if (substituted != aliased)
            return substituted;

        return type;
    }

    if (type->is<GenericType_ptr>())
    {
        auto generic_type = type->as<GenericType_ptr>();
        size_t i = 0;

        for (const auto& [name, obj] : templ->generics)
        {
            if (generic_type->name == name)
            {
                return generic_args[i];
            }
            i++;
        }

        return type;
    }

    if (type->is<ListType>())
    {
        return make_object(
            ListType{substitute_generics(type->as<ListType>().element_type, templ, generic_args)}
        );
    }

    if (type->is<SetType>())
    {
        return make_object(
            SetType{substitute_generics(type->as<SetType>().element_type, templ, generic_args)}
        );
    }

    if (type->is<MapType>())
    {
        auto& m = type->as<MapType>();
        return make_object(
            MapType{
                substitute_generics(m.key_type, templ, generic_args),
                substitute_generics(m.value_type, templ, generic_args)
            }
        );
    }

    if (type->is<TupleType>())
    {
        ObjectVector new_types;
        for (auto& t : type->as<TupleType>().element_types)
        {
            new_types.push_back(substitute_generics(t, templ, generic_args));
        }
        return make_object(TupleType{new_types});
    }

    if (type->is<VariantType>())
    {
        ObjectVector new_types;
        for (auto& t : type->as<VariantType>().types)
        {
            new_types.push_back(substitute_generics(t, templ, generic_args));
        }
        return make_object(VariantType{new_types});
    }

    if (type->is<FunctionType_ptr>())
    {
        auto func_type = type->as<FunctionType_ptr>();
        ObjectVector new_params;
        for (auto& p : func_type->parameter_types)
        {
            new_params.push_back(substitute_generics(p, templ, generic_args));
        }
        auto new_return = substitute_generics(func_type->return_type, templ, generic_args);

        return make_object(std::make_shared<FunctionType>(new_params, new_return));
    }

    if (type->is<ObjectOverloadList_ptr>())
    {
        auto overload_list = type->as<ObjectOverloadList_ptr>();
        auto new_list = std::make_shared<ObjectOverloadList>(overload_list->name);

        for (auto& overload : overload_list->overloads)
        {
            new_list->overloads.push_back(substitute_generics(overload, templ, generic_args));
        }

        return make_object(new_list);
    }

    return type;
}

Object_ptr TypeChecker::substitute_generics(
    FunctionTemplateType_ptr templ,
    const ObjectVector& generic_args
) const
{
    ObjectVector concrete_param_types;
    for (const auto& param_type : templ->signature->parameter_types)
    {
        concrete_param_types.push_back(substitute_generics(param_type, templ, generic_args));
    }

    Object_ptr concrete_return_type = substitute_generics(
        templ->signature->return_type,
        templ,
        generic_args
    );

    return make_object(std::make_shared<FunctionType>(concrete_param_types, concrete_return_type));
}

} // namespace Wasp
