#include "Doctor.h"
#include "Objects.h"
// required to remove symbol scope type incomplete error
#include "SymbolScope.h"
#include "TypeSystem.h"
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

SymbolVector::iterator TypeSystem::find_matching_signature(
    SymbolScope_ptr scope,
    SymbolVector& target_vector,
    const ObjectVector& parameter_types
)
{
    return std::find_if(
        target_vector.begin(),
        target_vector.end(),
        [&](const Symbol_ptr& symbol)
        {
            auto type = symbol->get_type();

            return type && type->is<Signature_ptr>() &&
                   equal(
                       scope,
                       type->as<Signature_ptr>()->parameter_types,
                       parameter_types
                   );
        }
    );
}

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
            if (signature->template_type->ordered_parameter_names.empty())
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

void TypeSystem::validate_new_function_overload(
    SymbolScope_ptr scope,
    std::string& name,
    const Symbol_ptr new_func
)
{
    auto existing = scope->lookup(name);
    if (!existing)
    {
        return;
    }

    auto type = new_func->get_type();
    Doctor::get().assert(
        type && type->is<Signature_ptr>(),
        WaspStage::Semantics,
        "Missing function type"
    );
    auto new_sig = type->as<Signature_ptr>();

    auto check_duplicate = [&](const Symbol_ptr& sibling)
    {
        if (sibling == new_func)
        {
            return;
        }
        auto s_type = sibling->get_type();
        if (s_type && s_type->is<Signature_ptr>() &&
            equal(
                scope,
                s_type->as<Signature_ptr>()->parameter_types,
                new_sig->parameter_types
            ))
        {
            Doctor::get().fatal(
                WaspStage::Semantics,
                "Duplicate signature for '" + name + "'"
            );
        }
    };

    if (existing->is<OverloadsSymbol>())
    {
        auto& group = existing->as<OverloadsSymbol>();
        for (const auto& sibling : group.overloads)
        {
            check_duplicate(sibling);
        }

        auto it = std::find_if(
            group.parents.begin(),
            group.parents.end(),
            [&](const Symbol_ptr& p)
            {
                auto pt = p->get_type();
                return pt && pt->is<Signature_ptr>() &&
                       equal(
                           scope,
                           pt->as<Signature_ptr>()->parameter_types,
                           new_sig->parameter_types
                       );
            }
        );
        if (it != group.parents.end())
        {
            group.parents.erase(it);
        }
    }
    else if (existing->is<FunctionSymbol>())
    {
        check_duplicate(existing);
    }
    else
    {
        Doctor::get().fatal(WaspStage::Semantics, "'" + name + "' cannot be overloaded");
    }
}

void TypeSystem::validate_new_method_overload(
    SymbolScope_ptr scope,
    ObjectVector existing,
    const Symbol_ptr new_method
)
{
    auto type = new_method->get_type();
    Doctor::get().assert(
        type && type->is<Signature_ptr>(),
        WaspStage::Semantics,
        "Missing method type"
    );
    auto new_sig = type->as<Signature_ptr>();

    for (const auto& overload : existing)
    {
        Doctor::get().assert(
            overload->is<Signature_ptr>(),
            WaspStage::Semantics,
            "Exepcted method overloads to be of type Signature"
        );

        Doctor::get().assert(
            equal(
                scope,
                overload->as<Signature_ptr>()->parameter_types,
                new_sig->parameter_types
            ),
            WaspStage::Semantics,
            "Duplicate method signature for '" + new_method->name + "'"
        );
    }
}

} // namespace Wasp
