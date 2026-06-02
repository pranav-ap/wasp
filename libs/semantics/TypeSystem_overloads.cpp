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
    // First, collect all viable candidates (assignable signatures)
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
            "Candidate '" + candidates[i]->name +
                "' should have a Signature type"
        );

        auto signature = type->as<Signature_ptr>();
        if (assignable(scope, signature->parameter_types, argument_types))
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

    // Helper to count total number of traits in a signature's parameters
    auto count_traits_in_params = [this](const Signature_ptr& sig) -> int
    {
        int count = 0;
        for (const auto& param_type : sig->parameter_types)
        {
            if (param_type->is<TraitType_ptr>())
            {
                count++;
            }
            else if (param_type->is<IntersectionType_ptr>())
            {
                auto inter = param_type->as<IntersectionType_ptr>();
                for (const auto& t : inter->types)
                {
                    if (t->is<TraitType_ptr>())
                    {
                        count++;
                    }
                }
            }
        }
        return count;
    };

    // Tie‑breaker 1: concrete (non‑template) beats template
    // Tie‑breaker 2: prefer signature with higher trait count in parameters
    // (more specific)

    // Separate candidates into concrete and template
    std::vector<size_t> concrete_positions;
    std::vector<size_t> template_positions;

    for (size_t i = 0; i < valid_matches.size(); ++i)
    {
        auto sig = valid_matches[i]->get_type()->as<Signature_ptr>();
        if (sig->template_type->exists())
        {
            template_positions.push_back(i);
        }
        else
        {
            concrete_positions.push_back(i);
        }
    }

    // Choose the set to work with: prefer concrete over template
    const auto& candidate_positions = concrete_positions.empty()
                                          ? template_positions
                                          : concrete_positions;

    if (candidate_positions.size() == 1)
    {
        size_t idx = candidate_positions.front();
        return {valid_matches[idx], match_indices[idx]};
    }

    // Multiple candidates in the preferred set → break tie by trait count
    int max_traits = -1;
    size_t best_idx = 0;
    for (size_t pos : candidate_positions)
    {
        auto sig = valid_matches[pos]->get_type()->as<Signature_ptr>();
        int traits = count_traits_in_params(sig);
        if (traits > max_traits)
        {
            max_traits = traits;
            best_idx = pos;
        }
        else if (traits == max_traits)
        {
            Doctor::get().fatal(
                WaspStage::Semantics,
                "Ambiguous call: multiple viable signatures with the same "
                "trait count"
            );
        }
    }

    return {valid_matches[best_idx], match_indices[best_idx]};
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
