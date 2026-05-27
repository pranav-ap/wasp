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

        if (auto generic_ptr = param_type->as<GenericType_ptr>())
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

        if (sig->template_type.has_value())
        {
            auto template_type = sig->template_type.value();

            if (template_type->ordered_parameter_names.size() ==
                expected_generic_count)
            {
                candidates.push_back({overloads[i], i});
            }
        }
    }

    return candidates;
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
        if (!v->is<GenericType_ptr>() || v->as<GenericType_ptr>()->name != k)
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
    std::unordered_map<Object_ptr, Object_ptr> memo;

    auto substitute_internal = [&](auto& self, Object_ptr t) -> Object_ptr
    {
        if (!t)
        {
            return nullptr;
        }

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
                [&](GenericType_ptr g) -> Object_ptr
                {
                    auto it = substitutions.find(g->name);
                    return it != substitutions.end() ? it->second : t;
                },

                [&](ClassType_ptr cls) -> Object_ptr
                {
                    // Build specialized name
                    std::string spec_name = cls->name;
                    ObjectVector substituted_types;
                    ObjectVector remaining_traits;
                    OptionalTemplateType new_template_type = std::nullopt;

                    // Track if any actual substitution happened
                    bool has_substitution = false;

                    // Process template parameters if they exist
                    if (cls->template_type.has_value())
                    {
                        auto template_type = cls->template_type.value();
                        GenericTypeMap new_template_params;
                        StringVector new_ordered_names;

                        // Build specialized name and collect substitutions
                        spec_name += "<";
                        bool first = true;

                        for (const auto& param_name :
                             template_type->ordered_parameter_names)
                        {
                            auto it = substitutions.find(param_name);
                            auto param_it = template_type->template_parameters
                                                .find(param_name);

                            if (it != substitutions.end())
                            {
                                // This parameter is being substituted
                                if (!first)
                                {
                                    spec_name += ", ";
                                }
                                spec_name += mangle_object(it->second);
                                substituted_types.push_back(it->second);
                                has_substitution = true;
                                first = false;
                            }
                            else if (
                                param_it !=
                                template_type->template_parameters.end()
                            )
                            {
                                // This parameter remains generic
                                new_template_params[param_name] = param_it
                                                                      ->second;
                                new_ordered_names.push_back(param_name);
                                if (!first)
                                {
                                    spec_name += ", ";
                                }
                                spec_name += param_name;
                                first = false;
                            }
                        }
                        spec_name += ">";

                        // Create new template if any parameters remain generic
                        if (!new_template_params.empty())
                        {
                            auto new_template = std::make_shared<Template>();
                            new_template
                                ->template_parameters = new_template_params;
                            new_template
                                ->ordered_parameter_names = new_ordered_names;
                            new_template_type = new_template;
                        }
                    }

                    // If no substitution happened, return original
                    if (!has_substitution && !new_template_type.has_value())
                    {
                        return t;
                    }

                    // Process traits (substitute any generic parameters in
                    // traits)
                    ObjectVector new_traits;
                    for (const auto& trait : cls->traits)
                    {
                        new_traits.push_back(sub(trait));
                    }

                    // Register shell object BEFORE recursing into members
                    auto new_cls = std::make_shared<ClassType>(
                        spec_name,         // name
                        ObjectStringMap{}, // members (to be filled)
                        cls->fields,       // fields
                        cls->methods,      // methods
                        cls->pures,        // pures
                        cls->statics,      // statics
                        new_traits,        // traits
                        new_template_type  // template_type
                    );

                    Object_ptr res = make_object(new_cls);
                    memo[t] = res;

                    // Now substitute all member types
                    for (const auto& [member_name, member_type] :
                         cls->member_types)
                    {
                        new_cls->member_types[member_name] = sub(member_type);
                    }

                    return res;
                },

                [&](TraitType_ptr trait) -> Object_ptr
                {
                    // Similar to ClassType but for traits
                    // (simplified - traits usually don't have data members)
                    if (!trait->template_type.has_value())
                    {
                        return t;
                    }

                    // Build specialized name
                    std::string spec_name = trait->name;
                    bool has_substitution = false;
                    OptionalTemplateType new_template_type = std::nullopt;

                    if (trait->template_type.has_value())
                    {
                        auto template_type = trait->template_type.value();
                        GenericTypeMap new_template_params;
                        StringVector new_ordered_names;

                        spec_name += "<";
                        bool first = true;

                        for (const auto& param_name :
                             template_type->ordered_parameter_names)
                        {
                            auto it = substitutions.find(param_name);

                            if (it != substitutions.end())
                            {
                                if (!first)
                                {
                                    spec_name += ", ";
                                }
                                spec_name += mangle_object(it->second);
                                has_substitution = true;
                                first = false;
                            }
                            else
                            {
                                auto param_it = template_type
                                                    ->template_parameters.find(
                                                        param_name
                                                    );
                                if (param_it !=
                                    template_type->template_parameters.end())
                                {
                                    new_template_params
                                        [param_name] = param_it->second;
                                    new_ordered_names.push_back(param_name);
                                    if (!first)
                                    {
                                        spec_name += ", ";
                                    }
                                    spec_name += param_name;
                                    first = false;
                                }
                            }
                        }
                        spec_name += ">";

                        if (!new_template_params.empty())
                        {
                            auto new_template = std::make_shared<Template>();
                            new_template
                                ->template_parameters = new_template_params;
                            new_template
                                ->ordered_parameter_names = new_ordered_names;
                            new_template_type = new_template;
                        }
                    }

                    if (!has_substitution && !new_template_type.has_value())
                    {
                        return t;
                    }

                    auto new_trait = std::make_shared<TraitType>(
                        spec_name,
                        ObjectStringMap{},
                        trait->fields,
                        trait->methods,
                        trait->pures,
                        trait->statics,
                        ObjectVector{}, // traits of traits (rare)
                        new_template_type
                    );

                    Object_ptr res = make_object(new_trait);
                    memo[t] = res;

                    // Substitute member types
                    for (const auto& [member_name, member_type] :
                         trait->member_types)
                    {
                        new_trait->member_types[member_name] = sub(member_type);
                    }

                    return res;
                },

                [&](Signature_ptr sig) -> Object_ptr
                {
                    auto new_sig = std::make_shared<Signature>(
                        ObjectVector{},
                        nullptr,
                        std::nullopt
                    );

                    Object_ptr res = make_object(new_sig);
                    memo[t] = res;

                    new_sig->parameter_types = sub_all(sig->parameter_types);
                    new_sig->return_type = sub(sig->return_type);

                    return res;
                },

                [&](FunctionOverloadsObject_ptr overloads) -> Object_ptr
                {
                    auto new_overloads = std::make_shared<
                        FunctionOverloadsObject>();
                    Object_ptr res = make_object(new_overloads);
                    memo[t] = res;
                    new_overloads->overloads = sub_all(overloads->overloads);
                    return res;
                },

                [&](TypeAlias_ptr alias) -> Object_ptr
                {
                    auto new_alias = std::make_shared<TypeAlias>(
                        alias->name,
                        sub(alias->underlying_type),
                        alias->template_type // Template params remain?
                    );
                    return make_object(new_alias);
                },

                [&](ListType_ptr list) -> Object_ptr
                {
                    auto new_list = std::make_shared<ListType>();
                    Object_ptr res = make_object(new_list);
                    memo[t] = res;
                    new_list->element_type = sub(list->element_type);
                    return res;
                },

                [&](MapType_ptr map) -> Object_ptr
                {
                    auto new_map = std::make_shared<MapType>();
                    Object_ptr res = make_object(new_map);
                    memo[t] = res;
                    new_map->key_type = sub(map->key_type);
                    new_map->value_type = sub(map->value_type);
                    return res;
                },

                [&](TupleType_ptr tuple) -> Object_ptr
                {
                    auto new_tuple = std::make_shared<TupleType>();
                    Object_ptr res = make_object(new_tuple);
                    memo[t] = res;
                    new_tuple->element_types = sub_all(tuple->element_types);
                    return res;
                },

                [&](VariantType_ptr variant) -> Object_ptr
                {
                    auto new_variant = std::make_shared<VariantType>();
                    Object_ptr res = make_object(new_variant);
                    memo[t] = res;
                    new_variant->types = sub_all(variant->types);
                    return res;
                },

                [&](IntersectionType_ptr intersection) -> Object_ptr
                {
                    auto
                        new_intersection = std::make_shared<IntersectionType>();
                    Object_ptr res = make_object(new_intersection);
                    memo[t] = res;
                    new_intersection->types = sub_all(intersection->types);
                    return res;
                },

                // Fallback for types that don't need substitution
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
