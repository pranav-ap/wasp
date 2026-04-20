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

// ==========================================================================
// VALIDATE OVERLOAD
// ==========================================================================

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

// ==========================================================================
// RESOLVE
// ==========================================================================

std::tuple<Symbol_ptr, int> TypeChecker::get_assignable_function_signatures(
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

std::vector<std::shared_ptr<Signature>> TypeChecker::get_assignable_function_signatures(
    SymbolScope_ptr scope,
    const std::vector<std::shared_ptr<Signature>>& candidates,
    const ObjectVector& argument_types
) const
{
    std::vector<std::shared_ptr<Signature>> valid_matches;

    for (const auto& candidate : candidates)
    {
        if (assignable(scope, candidate->parameter_types, argument_types))
        {
            valid_matches.push_back(candidate);
        }
    }

    return valid_matches;
}

std::tuple<Symbol_ptr, int> TypeChecker::resolve_class_method_call(
    SymbolScope_ptr scope,
    ClassType_ptr class_type,
    const std::string& method_name,
    const ObjectVector& argument_types
) const
{
    Doctor::get().assert(
        class_type->contains_member(method_name),
        WaspStage::Semantics,
        "Method '" + method_name + "()' does not exist on class '" + class_type->name + "'."
    );

    auto member = class_type->get_member(method_name);

    Doctor::get().assert(
        member->is<ObjectOverloadList>(),
        WaspStage::Semantics,
        "Member '" + method_name + "' of class '" + class_type->name +
            "' is not a method overload group."
    );

    const auto& object_overloads = member->as<ObjectOverloadList>();

    for (size_t i = 0; i < object_overloads.overloads.size(); ++i)
    {
        auto overload = object_overloads.overloads[i];
        auto function_signature = overload->try_as<MethodType>();

        if (assignable(scope, function_signature->parameter_types, argument_types))
        {
        }
    }
}

std::tuple<Symbol_ptr, Symbol_ptr, int, int> TypeChecker::resolve_module_function_call(
    SymbolScope_ptr scope,
    const std::string& module_name,
    const std::string& method_name,
    const ObjectVector& argument_types
) const
{
    Symbol_ptr module_symbol = scope->lookup(module_name);

    Doctor::get().fatal_if_nullptr(
        module_symbol,
        WaspStage::Semantics,
        "Module '" + module_name + "' not found in current scope"
    );

    Doctor::get().assert(
        module_symbol->payload_is<ModuleData>(),
        WaspStage::Semantics,
        "Symbol '" + module_name + "' is not a module"
    );

    auto& module_data = module_symbol->get_payload_as<ModuleData>();

    Symbol_ptr overload_group_symbol = module_data.mod->get_member(method_name);
    int module_member_index = module_data.mod->get_member_index(method_name);

    Doctor::get().fatal_if_nullptr(
        overload_group_symbol,
        WaspStage::Semantics,
        "Method '" + method_name + "' not found in module '" + module_name + "'"
    );

    Doctor::get().assert(
        overload_group_symbol->payload_is<FunctionOverloadsData>(),
        WaspStage::Semantics,
        "Symbol '" + method_name + "' is not an overload group"
    );

    const auto& group_data = overload_group_symbol->get_payload_as<FunctionOverloadsData>();

    SymbolVector valid_matches = get_assignable_function_signatures(
        scope,
        group_data.get_overloads(),
        argument_types
    );

    Doctor::get().assert(
        !valid_matches.empty(),
        WaspStage::Semantics,
        "No matching method signature found for '" + module_name + "." + method_name + "()'"
    );

    Doctor::get().assert(
        valid_matches.size() == 1,
        WaspStage::Semantics,
        "Ambiguous method call to '" + module_name + "." + method_name + "()'"
    );

    int overload_index = group_data.get_overload_index(valid_matches.front());

    // Returns: {Group Symbol, Specific Function Symbol, Overload Index, Module Member Index}
    return {overload_group_symbol, valid_matches.front(), overload_index, module_member_index};
}

} // namespace Wasp
