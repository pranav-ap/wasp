#include "Doctor.h"
#include "Objects.h"
#include "TypeSystem.h"
#include "Workspace.h"

#include <cstddef>
#include <memory>
#include <optional>
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
    const GenericTypeMap& params,
    bool& has_substitution,
    GenericTypeMap& remaining_params,
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

OptionalTemplateType make_template_if_needed(
    const GenericTypeMap& params,
    const StringVector& names
)
{
    if (params.empty())
    {
        return std::nullopt;
    }

    auto tmpl = std::make_shared<Template>();
    tmpl->template_parameters = params;
    tmpl->ordered_parameter_names = names;
    return tmpl;
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
                    if (!cls->template_type.has_value())
                    {
                        return t;
                    }

                    auto tmpl = cls->template_type.value();
                    bool has_sub = false;
                    GenericTypeMap remaining;
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

                    // Member types are now inside record_type
                    for (const auto& [name, type] :
                         cls->record_type.field_types)
                    {
                        new_cls->record_type.field_types[name] = sub(type);
                    }

                    return memo[t];
                },

                // TraitType handler
                [&](TraitType_ptr trait) -> Object_ptr
                {
                    if (!trait->template_type.has_value())
                    {
                        return t;
                    }

                    auto tmpl = trait->template_type.value();
                    bool has_sub = false;
                    GenericTypeMap remaining;
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
                        spec_name,              // name
                        trait->record_type,     // record_type
                        trait->bag_type,        // bag_type
                        sub_all(trait->traits), // traits
                        make_template_if_needed(
                            remaining,
                            remaining_names
                        ) // template_type
                    );

                    memo[t] = make_object(new_trait);

                    // Member types are inside record_type
                    for (const auto& [name, type] :
                         trait->record_type.field_types)
                    {
                        new_trait->record_type.field_types[name] = sub(type);
                    }

                    return memo[t];
                },

                // Signature
                [&](Signature_ptr sig) -> Object_ptr
                {
                    auto new_sig = std::make_shared<Signature>(
                        ObjectVector{},
                        nullptr
                    );
                    memo[t] = make_object(new_sig);
                    new_sig->parameter_types = sub_all(sig->parameter_types);
                    new_sig->return_type = sub(sig->return_type);
                    return memo[t];
                },

                // Pocket
                [&](Pocket_ptr os) -> Object_ptr
                {
                    auto new_os = std::make_shared<Pocket>();
                    memo[t] = make_object(new_os);
                    new_os->overloads = sub_all(os->overloads);
                    return memo[t];
                },

                // Type alias
                [&](TypeAlias_ptr alias) -> Object_ptr
                {
                    return make_object(
                        std::make_shared<TypeAlias>(
                            alias->name,
                            sub(alias->underlying_type),
                            alias->template_type
                        )
                    );
                },

                // Composite types
                [&](ListType_ptr list) -> Object_ptr
                {
                    auto new_list = std::make_shared<ListType>();
                    memo[t] = make_object(new_list);
                    new_list->element_type = sub(list->element_type);
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
