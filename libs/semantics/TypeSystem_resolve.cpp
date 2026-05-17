#include "Doctor.h"
#include "Objects.h"
#include "SymbolScope.h"
#include "TypeSystem.h"
#include "Workspace.h"

#include <algorithm>
#include <cstddef>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

// ============================================================================
// Overload Selection
// ============================================================================

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
            if (signature->expected_generic_names_order.empty())
            {
                concrete_count++;
                best_concrete = valid_matches[i];
                best_concrete_index = match_indices[i];
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
            Doctor::get()
                .assert(index == -1, WaspStage::Semantics, "Ambiguous call");

            best_match = candidates[i];
            index = static_cast<int>(i);
        }
    }

    Doctor::get()
        .assert(index != -1, WaspStage::Semantics, "No matching signature found");

    return {best_match, index};
}

// ============================================================================
// Semantic Validation
// ============================================================================

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

    if (existing->payload_is<OverloadsData>())
    {
        auto& group = existing->get_payload_as<OverloadsData>();
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
    else if (existing->payload_is<FunctionData>())
    {
        check_duplicate(existing);
    }
    else
    {
        Doctor::get().fatal(
            WaspStage::Semantics,
            "'" + name + "' cannot be overloaded"
        );
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
        if (overload->is<Signature_ptr>() &&
            equal(
                scope,
                overload->as<Signature_ptr>()->parameter_types,
                new_sig->parameter_types
            ))
        {
            Doctor::get().fatal(
                WaspStage::Semantics,
                "Duplicate method signature for '" + new_method->name + "'"
            );
        }
    }
}

// ============================================================================
// Transformations
// ============================================================================

std::vector<std::pair<Symbol_ptr, int>> TypeSystem::filter_by_generic_arity(
    const SymbolVector& overloads,
    size_t expected_generic_count
) const
{
    std::vector<std::pair<Symbol_ptr, int>> candidates;

    for (int i = 0; i < static_cast<int>(overloads.size()); ++i)
    {
        auto type = overloads[i]->get_type();
        if (!type || !type->is<Signature_ptr>())
        {
            continue;
        }

        auto sig = type->as<Signature_ptr>();
        if (sig->expected_generic_names_order.size() == expected_generic_count)
        {
            candidates.push_back({overloads[i], i});
        }
    }

    return candidates;
}

StringVector TypeSystem::get_generics_declaration_order(const Object_ptr& base) const
{
    return std::visit(
        overloaded{
            [&](const TemplateParameterType_ptr& g) -> StringVector
            {
                return {g->name};
            },
            [&](const auto& t) -> StringVector
            {
                // Only pull the vector for types that actually support generics
                if constexpr (requires { t->expected_generic_names_order; })
                {
                    return t->expected_generic_names_order;
                }
                return {};
            }
        },
        base->value
    );
}

TypeSystem::SpecializationResult TypeSystem::specialize_candidates(
    const std::vector<std::pair<Symbol_ptr, int>>& candidates,
    const ObjectVector& concrete_args
) const
{
    SpecializationResult result;

    for (const auto& [symbol, original_idx] : candidates)
    {
        auto type = symbol->get_type();
        auto names = get_generics_declaration_order(type);

        Doctor::get().assert(
            names.size() == concrete_args.size(),
            WaspStage::Semantics,
            "Generic arity mismatch for specialization of '" + symbol->name +
                "'. Expected " + std::to_string(names.size()) + " but got " +
                std::to_string(concrete_args.size()) + "."
        );

        ObjectStringMap substitutions;

        for (size_t i = 0; i < concrete_args.size(); ++i)
        {
            substitutions[names[i]] = concrete_args[i];
        }

        if (auto specialized = substitute_generics(type, substitutions))
        {
            result.signatures.push_back(std::move(specialized));
            result.original_indices.push_back(original_idx);
        }
    }

    return result;
}

Object_ptr TypeSystem::substitute_generics(
    Object_ptr type,
    const ObjectStringMap& substitutions
) const
{
    if (!type || substitutions.empty())
    {
        return type;
    }

    // 1. Identity Substitution Short-Circuit (T -> T)
    bool is_identity = true;
    for (const auto& [k, v] : substitutions)
    {
        if (!v->is<TemplateParameterType_ptr>() ||
            v->as<TemplateParameterType_ptr>()->name != k)
        {
            is_identity = false;
            break;
        }
    }
    if (is_identity)
    {
        return type;
    }

    // 2. Memoization map to break infinite recursion cycles
    // Key: Original pointer, Value: Specialized pointer
    std::unordered_map<Object_ptr, Object_ptr> memo;

    auto substitute_internal = [&](auto& self, Object_ptr t) -> Object_ptr
    {
        if (!t)
        {
            return nullptr;
        }

        // Check if we have already started substituting this specific pointer
        if (memo.contains(t))
        {
            return memo.at(t);
        }

        auto sub = [&](Object_ptr inner)
        {
            return self(self, inner);
        };
        auto sub_all = [&](const ObjectVector& types)
        {
            ObjectVector results;
            results.reserve(types.size());
            for (const auto& i : types)
            {
                results.push_back(sub(i));
            }
            return results;
        };

        return std::visit(
            overloaded{
                [&](TemplateParameterType_ptr g) -> Object_ptr
                {
                    return substitutions.contains(g->name)
                               ? substitutions.at(g->name)
                               : t;
                },
                [&](ClassType_ptr cls) -> Object_ptr
                {
                    // Create specialized name
                    std::string spec_name = cls->name + "<";
                    bool first = true, any_sub = false;
                    ObjectStringMap rem_gen;
                    StringVector rem_names;
                    for (const auto& gn : cls->expected_generic_names_order)
                    {
                        if (!substitutions.contains(gn))
                        {
                            rem_gen[gn] = cls->generics.at(gn);
                            rem_names.push_back(gn);
                        }
                        else
                        {
                            if (!first)
                            {
                                spec_name += ", ";
                            }
                            spec_name += mangle_object(substitutions.at(gn));
                            first = false;
                            any_sub = true;
                        }
                    }
                    spec_name += ">";

                    // CRITICAL: Register the shell object in the memo BEFORE
                    // recursing into members
                    auto new_cls = std::make_shared<ClassType>(
                        any_sub ? spec_name : cls->name,
                        ObjectStringMap{},
                        cls->fields,
                        cls->methods,
                        cls->pures,
                        cls->statics,
                        rem_gen,
                        rem_names
                    );
                    Object_ptr res = make_object(new_cls);
                    memo[t] = res;

                    // Now fill members. Circular references will hit the memo and
                    // return 'res'
                    for (auto const& [m_name, m_type] : cls->member_types)
                    {
                        new_cls->member_types[m_name] = sub(m_type);
                    }
                    return res;
                },
                [&](Signature_ptr sig) -> Object_ptr
                {
                    auto new_sig = std::make_shared<Signature>(
                        ObjectVector{},
                        nullptr,
                        ObjectStringMap{},
                        StringVector{}
                    );
                    Object_ptr res = make_object(new_sig);
                    memo[t] = res;
                    new_sig->parameter_types = sub_all(sig->parameter_types);
                    new_sig->return_type = sub(sig->return_type);
                    return res;
                },
                [&](ObjectOverloadList_ptr list) -> Object_ptr
                {
                    auto new_list = std::make_shared<ObjectOverloadList>();
                    Object_ptr res = make_object(new_list);
                    memo[t] = res;
                    new_list->overloads = sub_all(list->overloads);
                    return res;
                },
                // Fallback for standard types (int, str, etc.) so they aren't erased
                [&](auto&) -> Object_ptr
                {
                    return t;
                }
            },
            t->value
        );
    };

    return substitute_internal(substitute_internal, type);
}

Object_ptr TypeSystem::resolve_type(Object_ptr type, bool resolve_generics) const
{
    while (type && type->is<TypeAlias_ptr>())
    {
        type = type->as<TypeAlias_ptr>()->underlying_type;
    }

    if (resolve_generics && type && type->is<TemplateParameterType_ptr>())
    {
        type = type->as<TemplateParameterType_ptr>()->constraint_type;
    }

    return type;
}

} // namespace Wasp
