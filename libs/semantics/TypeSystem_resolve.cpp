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
        if (type && type->is<Signature_ptr>())
        {
            auto sig = type->as<Signature_ptr>();
            if (assignable(scope, sig->parameter_types, argument_types))
            {
                valid_matches.push_back(candidates[i]);
                match_indices.push_back(static_cast<int>(i));
            }
        }
    }

    Doctor::get().assert(
        !valid_matches.empty(),
        WaspStage::Semantics,
        "No matching signature found"
    );

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

Object_ptr TypeSystem::substitute_generics(
    Object_ptr type,
    const ObjectStringMap& substitutions
) const
{
    if (!type || substitutions.empty())
    {
        return type;
    }

    auto substitute_internal = [&](auto& self, Object_ptr t) -> Object_ptr
    {
        if (!t)
        {
            return nullptr;
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
                [&](GenericType_ptr g) -> Object_ptr
                {
                    // Core Replacement Logic
                    if (substitutions.contains(g->name))
                    {
                        return substitutions.at(g->name);
                    }
                    return t;
                },
                [&](TypeAlias_ptr ta) -> Object_ptr
                {
                    ObjectStringMap rem_gen;
                    StringVector rem_names;
                    for (const auto& gn : ta->expected_generic_names_order)
                    {
                        if (!substitutions.contains(gn))
                        {
                            rem_gen[gn] = ta->generics.at(gn);
                            rem_names.push_back(gn);
                        }
                    }
                    return make_object(
                        std::make_shared<TypeAlias>(
                            ta->name,
                            sub(ta->underlying_type),
                            rem_gen,
                            rem_names
                        )
                    );
                },
                [&](Signature_ptr sig) -> Object_ptr
                {
                    ObjectStringMap rem_gen;
                    StringVector rem_names;
                    for (const auto& gn : sig->expected_generic_names_order)
                    {
                        if (!substitutions.contains(gn))
                        {
                            rem_gen[gn] = sig->generics.at(gn);
                            rem_names.push_back(gn);
                        }
                    }
                    return make_object(
                        std::make_shared<Signature>(
                            sub_all(sig->parameter_types),
                            sub(sig->return_type),
                            rem_gen,
                            rem_names
                        )
                    );
                },
                [&](ClassType_ptr cls) -> Object_ptr
                {
                    ObjectStringMap concrete_members;
                    for (auto const& [m_name, m_type] : cls->member_types)
                    {
                        concrete_members[m_name] = sub(m_type);
                    }

                    ObjectStringMap rem_gen;
                    StringVector rem_names;
                    for (const auto& gn : cls->expected_generic_names_order)
                    {
                        if (!substitutions.contains(gn))
                        {
                            rem_gen[gn] = cls->generics.at(gn);
                            rem_names.push_back(gn);
                        }
                    }
                    return make_object(
                        std::make_shared<ClassType>(
                            cls->name,
                            std::move(concrete_members),
                            cls->fields,
                            cls->methods,
                            cls->pures,
                            cls->statics,
                            rem_gen,
                            rem_names
                        )
                    );
                },
                [&](TraitType_ptr trt) -> Object_ptr
                {
                    ObjectStringMap concrete_members;
                    for (auto const& [m_name, m_type] : trt->member_types)
                    {
                        concrete_members[m_name] = sub(m_type);
                    }

                    ObjectStringMap rem_gen;
                    StringVector rem_names;
                    for (const auto& gn : trt->expected_generic_names_order)
                    {
                        if (!substitutions.contains(gn))
                        {
                            rem_gen[gn] = trt->generics.at(gn);
                            rem_names.push_back(gn);
                        }
                    }
                    return make_object(
                        std::make_shared<TraitType>(
                            trt->name,
                            std::move(concrete_members),
                            trt->methods,
                            trt->pures,
                            trt->statics,
                            rem_gen,
                            rem_names
                        )
                    );
                },
                [&](ListType& l) -> Object_ptr
                {
                    return make_object(ListType{sub(l.element_type)});
                },
                [&](SetType& s) -> Object_ptr
                {
                    return make_object(SetType{sub(s.element_type)});
                },
                [&](MapType& m) -> Object_ptr
                {
                    return make_object(MapType{sub(m.key_type), sub(m.value_type)});
                },
                [&](TupleType& tu) -> Object_ptr
                {
                    return make_object(TupleType{sub_all(tu.element_types)});
                },
                [&](VariantType& v) -> Object_ptr
                {
                    return make_object(VariantType{sub_all(v.types)});
                },
                [&](ObjectOverloadList_ptr list) -> Object_ptr
                {
                    auto new_list = std::make_shared<ObjectOverloadList>();
                    new_list->overloads = sub_all(list->overloads);
                    return make_object(new_list);
                },
                [](auto&) -> Object_ptr
                {
                    return nullptr;
                }
            },
            t->value
        );
    };

    auto result = substitute_internal(substitute_internal, type);
    return result ? result : type;
}

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

std::tuple<ObjectStringMap, StringVector, std::string> TypeSystem::
    extract_generics_and_names(const Object_ptr& base) const
{
    using ReturnType = std::tuple<ObjectStringMap, StringVector, std::string>;

    return std::visit(
        overloaded{
            [&](const ClassType_ptr& c) -> ReturnType
            {
                return {c->generics, c->expected_generic_names_order, c->name};
            },
            [&](const TraitType_ptr& t) -> ReturnType
            {
                return {t->generics, t->expected_generic_names_order, t->name};
            },
            [&](const TypeAlias_ptr& a) -> ReturnType
            {
                return {a->generics, a->expected_generic_names_order, a->name};
            },
            [&](const Signature_ptr& s) -> ReturnType
            {
                return {s->generics, s->expected_generic_names_order, "fun"};
            },
            [&](const GenericType_ptr& g) -> ReturnType
            {
                return {{{g->name, g->constraint_type}}, {g->name}, g->name};
            },
            [](const auto&) -> ReturnType
            {
                return {{}, {}, ""};
            },
        },
        base->value
    );
}

StringVector TypeSystem::get_generics_declaration_order(const Object_ptr& base) const
{
    return std::visit(
        overloaded{
            [&](const GenericType_ptr& g) -> StringVector
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

} // namespace Wasp
