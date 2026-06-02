#include "Doctor.h"
#include "Objects.h"
// required to remove symbol scope type incomplete error
#include "SymbolScope.h"
#include "TypeSystem.h"
#include "Workspace.h"

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

std::tuple<Symbol_ptr, int> TypeSystem::get_best_function_symbol(
    SymbolScope_ptr scope,
    const SymbolVector& candidates,
    const ObjectVector& argument_types
) const
{
    SymbolVector valid_matches;
    std::vector<int> match_indices;

    for (size_t i = 0; i < candidates.size(); ++i)
    {
        auto type = candidates[i]->get_type();

        Doctor::get().fatal_if_nullptr(
            type,
            WaspStage::Semantics,
            "Candidate '" + candidates[i]->name + "' is missing a type"
        );

        Doctor::get().assert(
            type->is<Signature_ptr>(),
            WaspStage::Semantics,
            "Candidate '" + candidates[i]->name + "' should have a Signature type"
        );

        auto signature = type->as<Signature_ptr>();

        bool is_assignable = assignable(
            scope,
            signature->parameter_types,
            argument_types
        );

        if (is_assignable)
        {
            valid_matches.push_back(candidates[i]);
            match_indices.push_back(static_cast<int>(i));
        }
    }

    Doctor::get().assert(
        !valid_matches.empty(),
        WaspStage::Semantics,
        "No matching signature found"
    );

    // ========================================================================
    // Tie-Breaker: Concrete > Template
    // ========================================================================
    if (valid_matches.size() > 1)
    {
        Symbol_ptr best_concrete = nullptr;
        int best_concrete_index = -1;
        int concrete_count = 0;

        for (size_t i = 0; i < valid_matches.size(); ++i)
        {
            auto signature = valid_matches[i]->get_type()->as<Signature_ptr>();

            // If the signature has no expected generics, it is purely concrete
            if (!signature->template_type->exists())
            {
                best_concrete = valid_matches[i];
                best_concrete_index = match_indices[i];
                concrete_count++;
            }
        }

        if (concrete_count == 1)
        {
            return {best_concrete, best_concrete_index};
        }

        Doctor::get().fatal(WaspStage::Semantics, "Ambiguous call");
    }

    Doctor::get()
        .assert(valid_matches.size() == 1, WaspStage::Semantics, "Ambiguous call");

    return {valid_matches.front(), match_indices.front()};
}

std::tuple<Object_ptr, int> TypeSystem::get_best_function_object(
    SymbolScope_ptr scope,
    const ObjectVector& candidates,
    const ObjectVector& argument_types
) const
{
    Object_ptr best_match = nullptr;
    int index = -1;

    for (size_t i = 0; i < candidates.size(); ++i)
    {
        auto signature = candidates[i]->as<Signature_ptr>();
        if (assignable(scope, signature->parameter_types, argument_types))
        {
            Doctor::get().assert(index == -1, WaspStage::Semantics, "Ambiguous call");

            best_match = candidates[i];
            index = static_cast<int>(i);
        }
    }

    Doctor::get()
        .assert(index != -1, WaspStage::Semantics, "No matching signature found");

    return {best_match, index};
}

std::tuple<Object_ptr, int> TypeSystem::get_possible_best_function_object(
    SymbolScope_ptr scope,
    const ObjectVector& candidates,
    const ObjectVector& argument_types
) const
{
    Object_ptr best_match = nullptr;
    int index = -1;

    for (size_t i = 0; i < candidates.size(); ++i)
    {
        auto signature = candidates[i]->as<Signature_ptr>();
        if (assignable(scope, signature->parameter_types, argument_types))
        {
            Doctor::get()
                .assert(index == -1, WaspStage::Semantics, "Ambiguous call");

            best_match = candidates[i];
            index = static_cast<int>(i);
        }
    }

    return {best_match, index};
}

} // namespace Wasp
