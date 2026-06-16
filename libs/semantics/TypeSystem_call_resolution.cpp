#include "AST.h"
#include "Doctor.h"
#include "Symbol.h"
#include "SymbolScope.h"
#include "Type.h"
#include "TypeSystem.h"

#include <algorithm>
#include <cstddef>
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
    const Symbol_ptr symbol,
    const TypeVector& generic_types,
    const TypeVector& argument_types
) const
{
    Doctor::semantics().assert(
        symbol->is<FunctionOverloadsSymbol>(),
        "Symbol '" + symbol->name + "' is not an overloaded function"
    );

    auto& func_overloads_payload = symbol->as<FunctionOverloadsSymbol>();
    const auto& candidates = func_overloads_payload.overloads;

    struct Candidate
    {
        Symbol_ptr sym;
        int index;
        Signature_ptr sig;
        int score;
    };

    std::vector<Candidate> viable;

    for (size_t i = 0; i < candidates.size(); ++i)
    {
        auto& function_symbol_obj = candidates[i];
        auto& function_symbol = function_symbol_obj->as<FunctionSymbol>();
        auto signature = function_symbol.type->as<Signature_ptr>();

        // Skip if arity doesn't match
        if (signature->parameter_types.size() != argument_types.size())
        {
            continue;
        }

        // Check if arguments are assignable to parameters
        bool all_assignable = true;

        for (size_t j = 0; j < argument_types.size(); ++j)
        {
            bool is_assignable = assignable(
                scope,
                signature->parameter_types[j],
                argument_types[j]
            );

            if (!is_assignable)
            {
                all_assignable = false;
                break;
            }
        }

        if (all_assignable)
        {
            viable.push_back(
                {function_symbol_obj, static_cast<int>(i), signature, 0}
            );
        }
    }

    Doctor::semantics().assert(
        !viable.empty(),
        "No viable candidates for function " + symbol->name
    );

    // Only one candidate.
    // Return it.
    if (viable.size() == 1)
    {
        return {viable[0].sym, viable[0].index};
    }

    // Multiple candidates.
    // Score them.
    // Higher score = better match
    for (auto& c : viable)
    {
        // Prefer non-generic over generic
        if (!c.sig->template_type || c.sig->template_type->empty())
        {
            c.score += 10;
        }

        // Prefer exact type matches (higher specificity)
        for (size_t i = 0; i < argument_types.size(); ++i)
        {
            if (equal(scope, c.sig->parameter_types[i], argument_types[i]))
            {
                c.score += 5;
            }
        }
    }

    // Sort by score in descending order
    std::sort(
        viable.begin(),
        viable.end(),
        [](const Candidate& a, const Candidate& b)
        {
            return a.score > b.score;
        }
    );

    // Check for ambiguity (two candidates with same highest score)
    if (viable.size() > 1 && viable[0].score == viable[1].score)
    {
        Doctor::semantics().fatal(
            "Ambiguous call to '" + symbol->name + "'"
        );
    }

    // Return the best match
    return {viable[0].sym, viable[0].index};
}

} // namespace Wasp
