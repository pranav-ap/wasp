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
    auto overload_group_symbol = scope->lookup(function_name);

    if (!overload_group_symbol)
    {
        return;
    }

    Doctor::get().assert(
        overload_group_symbol->payload_is<FunctionOverloadsData>(),
        WaspStage::Semantics,
        "Symbol is not an overload group"
    );

    auto new_function_signature = get_function_signature(new_function_symbol->get_type());

    Doctor::get().fatal_if_nullptr(
        new_function_signature,
        WaspStage::Semantics,
        "New function symbol lacks a valid function type"
    );

    const auto& parameter_types = new_function_signature->parameter_types;
    auto& group_data = overload_group_symbol->get_payload_as<FunctionOverloadsData>();

    // Sibling Duplicate Check
    for (const auto& sibling : group_data.siblings)
    {
        if (sibling == new_function_symbol)
        {
            continue;
        }

        auto type_obj = sibling->get_type();

        // If the sibling hasn't been type-checked yet (only hoisted so far), skip it.
        if (!type_obj)
        {
            continue;
        }

        auto existing_signature = get_function_signature(type_obj);

        Doctor::get().assert(
            existing_signature != nullptr,
            WaspStage::Semantics,
            "Overload group contains a non-function symbol"
        );

        Doctor::get().assert(
            !equal(scope, existing_signature->parameter_types, parameter_types),
            WaspStage::Semantics,
            "Duplicate function signatures for '" + function_name + "' defined in same scope"
        );
    }

    // Shadow Parents
    auto parent_match = find_matching_signature(scope, group_data.parents, parameter_types);

    if (parent_match != group_data.parents.end())
    {
        group_data.parents.erase(parent_match);
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

} // namespace Wasp
