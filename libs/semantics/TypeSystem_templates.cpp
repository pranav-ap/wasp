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
) const
{
    ObjectStringMap substitutions;

    for (size_t i = 0; i < signature->parameter_types.size(); ++i)
    {
        auto param_type = signature->parameter_types[i];
        param_type = resolve_type(param_type, false);

        if (param_type->is<GenericType_ptr>())
        {
            auto generic = param_type->as<GenericType_ptr>();
            substitutions[generic->name] = argument_types[i];
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

        if (sig->template_type)
        {
            if (sig->template_type->ordered_parameter_names.size() ==
                expected_generic_count)
            {
                candidates.push_back({overloads[i], i});
            }
        }
    }

    return candidates;
}

StringVector TypeSystem::get_generics_declaration_order(
    const Object_ptr& base
) const
{
    return std::visit(
        overloaded{
            [&](GenericType_ptr g) -> StringVector
            {
                return {g->name};
            },
            [&](ClassType_ptr cls) -> StringVector
            {
                if (cls->template_type)
                {
                    return cls->template_type->ordered_parameter_names;
                }
                return {};
            },
            [&](TraitType_ptr trait) -> StringVector
            {
                if (trait->template_type)
                {
                    return trait->template_type->ordered_parameter_names;
                }
                return {};
            },
            [&](Signature_ptr sig) -> StringVector
            {
                if (sig->template_type)
                {
                    return sig->template_type->ordered_parameter_names;
                }
                return {};
            },
            [&](const auto&) -> StringVector
            {
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

namespace
{
// Helper to check if a substitution is identity
bool is_identity_substitution(const ObjectStringMap& subs)
{
    for (const auto& [name, value] : subs)
    {
        if (!value->is<GenericType_ptr>() ||
            value->as<GenericType_ptr>()->name != name)
        {
            return false;
        }
    }
    return true;
}

// Helper to build specialized name for generic types
std::string build_specialized_name(
    const std::string& base_name,
    const ObjectStringMap& subs,
    const StringVector& param_names,
    const ObjectStringMap& params,
    bool& has_substitution,
    ObjectStringMap& remaining_params,
    StringVector& remaining_names
)
{
    std::string result = base_name + "<";
    bool first = true;

    for (const auto& param_name : param_names)
    {
        auto it = subs.find(param_name);
        auto param_it = params.find(param_name);

        if (it != subs.end())
        {
            if (!first)
            {
                result += ", ";
            }
            result += Object::mangle_object(it->second);
            has_substitution = true;
            first = false;
        }
        else if (param_it != params.end())
        {
            remaining_params[param_name] = param_it->second;
            remaining_names.push_back(param_name);
            if (!first)
            {
                result += ", ";
            }
            result += param_name;
            first = false;
        }
    }
    result += ">";
    return result;
}

TemplateType_ptr make_template_if_needed(
    const ObjectStringMap& params,
    const StringVector& names
)
{
    if (params.empty())
    {
        return nullptr;
    }
    return std::make_shared<TemplateType>(params, names);
}
} // namespace

Object_ptr TypeSystem::substitute_generics(
    Object_ptr type,
    const ObjectStringMap& subs
) const
{
    if (!type || subs.empty())
    {
        return type;
    }
    if (is_identity_substitution(subs))
    {
        return type;
    }

    std::unordered_map<Object_ptr, Object_ptr> memo;

    auto visit = [&](auto&& self, Object_ptr t) -> Object_ptr
    {
        if (!t)
        {
            return nullptr;
        }
        if (auto it = memo.find(t); it != memo.end())
        {
            return it->second;
        }

        auto sub = [&](Object_ptr x)
        {
            return self(self, x);
        };
        auto sub_all = [&](const ObjectVector& vec)
        {
            ObjectVector out;
            out.reserve(vec.size());
            for (const auto& x : vec)
            {
                out.push_back(sub(x));
            }
            return out;
        };

        return std::visit(
            overloaded{
                // Generic parameter
                [&](GenericType_ptr g) -> Object_ptr
                {
                    auto it = subs.find(g->name);
                    return it != subs.end() ? it->second : t;
                },

                // ClassType handler
                [&](ClassType_ptr cls) -> Object_ptr
                {
                    if (!cls->template_type)
                    {
                        return t;
                    }

                    auto tmpl = cls->template_type;
                    bool has_sub = false;
                    ObjectStringMap remaining;
                    StringVector remaining_names;

                    auto spec_name = build_specialized_name(
                        cls->name,
                        subs,
                        tmpl->ordered_parameter_names,
                        tmpl->template_parameters,
                        has_sub,
                        remaining,
                        remaining_names
                    );

                    if (!has_sub && remaining.empty())
                    {
                        return t;
                    }

                    auto new_cls = std::make_shared<ClassType>(
                        spec_name,
                        cls->record_type,
                        cls->bag_type,
                        sub_all(cls->traits),
                        make_template_if_needed(remaining, remaining_names)
                    );

                    memo[t] = make_object(new_cls);

                    // Substitute field types
                    for (const auto& [name, field_type] :
                         cls->record_type.field_types)
                    {
                        new_cls->record_type.field_types[name] = sub(
                            field_type
                        );
                    }
                    new_cls->record_type.ordered_keys = cls->record_type
                                                            .ordered_keys;

                    // Substitute method types (signatures)
                    for (const auto& [name, method_type] :
                         cls->bag_type.overload_types)
                    {
                        new_cls->bag_type.overload_types[name] = sub(
                            method_type
                        );
                    }
                    new_cls->bag_type.ordered_keys = cls->bag_type.ordered_keys;
                    new_cls->bag_type.itables = cls->bag_type.itables;

                    return memo[t];
                },

                // TraitType handler
                [&](TraitType_ptr trait) -> Object_ptr
                {
                    if (!trait->template_type)
                    {
                        return t;
                    }

                    auto tmpl = trait->template_type;
                    bool has_sub = false;
                    ObjectStringMap remaining;
                    StringVector remaining_names;

                    auto spec_name = build_specialized_name(
                        trait->name,
                        subs,
                        tmpl->ordered_parameter_names,
                        tmpl->template_parameters,
                        has_sub,
                        remaining,
                        remaining_names
                    );

                    if (!has_sub && remaining.empty())
                    {
                        return t;
                    }

                    auto new_trait = std::make_shared<TraitType>(
                        spec_name,
                        trait->record_type,
                        trait->bag_type,
                        sub_all(trait->traits),
                        make_template_if_needed(remaining, remaining_names)
                    );

                    memo[t] = make_object(new_trait);

                    // Substitute field types
                    for (const auto& [name, field_type] :
                         trait->record_type.field_types)
                    {
                        new_trait->record_type.field_types[name] = sub(
                            field_type
                        );
                    }
                    new_trait->record_type.ordered_keys = trait->record_type
                                                              .ordered_keys;

                    // Substitute method types
                    for (const auto& [name, method_type] :
                         trait->bag_type.overload_types)
                    {
                        new_trait->bag_type.overload_types[name] = sub(
                            method_type
                        );
                    }
                    new_trait->bag_type.ordered_keys = trait->bag_type
                                                           .ordered_keys;
                    new_trait->bag_type.itables = trait->bag_type.itables;

                    return memo[t];
                },

                // Signature
                [&](Signature_ptr sig) -> Object_ptr
                {
                    auto new_sig = std::make_shared<Signature>(
                        ObjectVector{},
                        nullptr,
                        sig->template_type
                    );
                    memo[t] = make_object(new_sig);
                    new_sig->parameter_types = sub_all(sig->parameter_types);
                    new_sig->return_type = sub(sig->return_type);
                    return memo[t];
                },

                // Pocket (OverloadSet)
                [&](Pocket_ptr pocket) -> Object_ptr
                {
                    auto new_pocket = std::make_shared<Pocket>();
                    memo[t] = make_object(new_pocket);
                    new_pocket->overloads = sub_all(pocket->overloads);
                    return memo[t];
                },

                // Type alias
                [&](TypeAlias_ptr alias) -> Object_ptr
                {
                    auto new_alias = std::make_shared<TypeAlias>(
                        alias->name,
                        alias->template_type,
                        sub(alias->underlying_type)
                    );
                    return make_object(new_alias);
                },

                // Composite types
                [&](ListType_ptr list) -> Object_ptr
                {
                    auto new_list = std::make_shared<ListType>();
                    memo[t] = make_object(new_list);
                    new_list->element_type = sub(list->element_type);
                    return memo[t];
                },

                [&](SetType_ptr set) -> Object_ptr
                {
                    auto new_set = std::make_shared<SetType>();
                    memo[t] = make_object(new_set);
                    new_set->element_type = sub(set->element_type);
                    return memo[t];
                },

                [&](MapType_ptr map) -> Object_ptr
                {
                    auto new_map = std::make_shared<MapType>();
                    memo[t] = make_object(new_map);
                    new_map->key_type = sub(map->key_type);
                    new_map->value_type = sub(map->value_type);
                    return memo[t];
                },

                [&](TupleType_ptr tuple) -> Object_ptr
                {
                    auto new_tuple = std::make_shared<TupleType>();
                    memo[t] = make_object(new_tuple);
                    new_tuple->element_types = sub_all(tuple->element_types);
                    return memo[t];
                },

                [&](VariantType_ptr var) -> Object_ptr
                {
                    auto new_var = std::make_shared<VariantType>();
                    memo[t] = make_object(new_var);
                    new_var->types = sub_all(var->types);
                    return memo[t];
                },

                [&](IntersectionType_ptr inter) -> Object_ptr
                {
                    auto new_inter = std::make_shared<IntersectionType>();
                    memo[t] = make_object(new_inter);
                    new_inter->types = sub_all(inter->types);
                    return memo[t];
                },

                // Fallback - no substitution needed
                [&](auto&) -> Object_ptr
                {
                    return t;
                }
            },
            t->value
        );
    };

    return visit(visit, type);
}

} // namespace Wasp
