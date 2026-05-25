#include "Doctor.h"
#include "Objects.h"
#include "TypeSystem.h"
#include "Workspace.h"

#include <cstddef>
#include <memory>
#include <string>
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

ObjectStringMap TypeSystem::infer_template_arguments(
    Signature_ptr signature,
    const ObjectVector& argument_types
)
{
    ObjectStringMap substitutions;

    for (size_t i = 0; i < signature->parameter_types.size(); ++i)
    {
        auto param_type = signature->parameter_types[i];
        param_type = resolve_type(param_type, false);

        if (auto generic_ptr = param_type->as<TemplateParameterType_ptr>())
        {
            substitutions[generic_ptr->name] = argument_types[i];
        }
    }

    return substitutions;
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
        if (sig->ordered_template_parameter_names.size() == expected_generic_count)
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
                    return substitutions.contains(g->name) ? substitutions.at(g->name)
                                                           : t;
                },
                [&](ClassType_ptr cls) -> Object_ptr
                {
                    // Create specialized name
                    std::string spec_name = cls->name + "<";
                    bool first = true, any_sub = false;
                    ObjectStringMap rem_gen;
                    StringVector rem_names;
                    for (const auto& gn : cls->ordered_template_parameter_names)
                    {
                        if (!substitutions.contains(gn))
                        {
                            rem_gen[gn] = cls->template_parameter_types.at(gn);
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

} // namespace Wasp
