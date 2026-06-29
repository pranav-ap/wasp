#include "AST.h"
#include "Doctor.h"
#include "Expression.h"
#include "SemanticsAnalyzer.h"
#include "Symbol.h"
#include "Type.h"
#include "TypeSystem.h"

#include <cstddef>
#include <optional>
#include <string>
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

Type_ptr SemanticsAnalyzer::visit(Constructor& cons)
{
    TypeVector solid_types = visit(cons.angular_nodes);
    TypeVector argument_types = visit(cons.arguments);

    return std::visit(
        overloaded{
            [&](Identifier& id) -> Type_ptr
            {
                Symbol_ptr symbol = current_scope->lookup_required_and_resolve(id.name);

                auto [result_type, resolved_symbol] = resolve_constructor_from_symbol(
                    symbol,
                    solid_types,
                    argument_types
                );

                id.symbol = resolved_symbol;
                id.must_be_captured = resolved_symbol->should_be_captured(current_scope->closure_depth);

                return result_type;
            },
            [&](MemberAccess& ma) -> Type_ptr
            {
                Type_ptr owner_type = visit(ma.owner);
                owner_type = owner_type->unwrap_alias();

                Doctor::semantics().check(
                    owner_type->is<ModuleType_ptr>(),
                    "Constructor can only be called on a module type"
                );

                ModuleType_ptr mod_type = owner_type->as<ModuleType_ptr>();
                Module_ptr mod = workspace->get_module(mod_type->absolute_filepath);
                Doctor::semantics().fatal_if_nullptr(mod);

                Doctor::semantics().check(ma.member->is<Identifier>(), "Module member must be an identifier");

                std::string member_name = ma.member->as<Identifier>().name;
                int member_index = mod_type->get_member_index(member_name);
                Symbol_ptr member_symbol = mod->exported_symbols[member_index];
                Doctor::semantics().fatal_if_nullptr(member_symbol);

                auto [result_type, resolved_symbol] = resolve_constructor_from_symbol(
                    member_symbol,
                    solid_types,
                    argument_types
                );

                return result_type;
            },
            [&](auto&) -> Type_ptr
            {
                Doctor::semantics().fatal("Invalid constructible");
            }
        },
        cons.constructible->data
    );
}

std::optional<TypeSubstitutionMap> SemanticsAnalyzer::deduce_class_template_arguments(
    ClassType_ptr class_type,
    const TypeVector& argument_types
) const
{
    if (class_type->template_type->empty())
    {
        return std::nullopt;
    }

    const StringVector& param_names = class_type->template_type->ordered_parameter_names;
    const TypeVector& field_types = class_type->fields->get_ordered_types();

    if (argument_types.size() != field_types.size())
    {
        return std::nullopt;
    }

    TypeSubstitutionMap substitutions;
    for (size_t i = 0; i < field_types.size(); ++i)
    {
        const Type_ptr& field_type = field_types[i];
        if (field_type->is<GenericType_ptr>())
        {
            auto generic = field_type->as<GenericType_ptr>();
            const std::string& param_name = generic->name;
            const Type_ptr& arg_type = argument_types[i];

            if (generic->constraint_type &&
                !type_system->assignable(current_scope, generic->constraint_type, arg_type))
            {
                return std::nullopt;
            }

            auto it = substitutions.find(param_name);
            if (it != substitutions.end())
            {
                if (!type_system->equal(current_scope, it->second, arg_type))
                {
                    return std::nullopt;
                }
            }
            else
            {
                substitutions[param_name] = arg_type;
            }
        }
        else
        {
            if (!type_system->assignable(current_scope, field_type, argument_types[i]))
            {
                return std::nullopt;
            }
        }
    }

    for (const std::string& name : param_names)
    {
        if (substitutions.find(name) == substitutions.end())
        {
            return std::nullopt;
        }
    }

    return substitutions;
}

std::pair<Type_ptr, Symbol_ptr> SemanticsAnalyzer::resolve_constructor_from_symbol(
    Symbol_ptr symbol,
    const TypeVector& solid_types,
    const TypeVector& argument_types
)
{
    SymbolVector candidates;

    if (symbol->is<OverloadSymbol>())
    {
        candidates = symbol->as<OverloadSymbol>().overloads;
    }
    else
    {
        candidates.push_back(symbol);
    }

    for (const Symbol_ptr& cand : candidates)
    {
        Type_ptr type = cand->get_type();

        if (!type->is<ClassType_ptr>())
        {
            continue;
        }

        auto cls = type->as<ClassType_ptr>();

        if (cls->template_type && !cls->template_type->empty())
        {
            continue;
        }

        if (cls->fields->ordered_keys.size() != argument_types.size())
        {
            continue;
        }

        bool all_assignable = true;

        for (size_t i = 0; i < argument_types.size(); ++i)
        {
            if (!type_system->assignable(current_scope, cls->fields->get_type(i), argument_types[i]))
            {
                all_assignable = false;
                break;
            }
        }
        if (all_assignable)
        {
            return {type, cand};
        }
    }

    for (const auto& cand : candidates)
    {
        Type_ptr type = cand->get_type();

        if (!type->is<ClassType_ptr>())
        {
            continue;
        }

        auto cls = type->as<ClassType_ptr>();

        if (!cls->template_type || cls->template_type->empty())
        {
            continue;
        }

        if (!solid_types.empty())
        {
            const auto& param_names = cls->template_type->ordered_parameter_names;

            if (solid_types.size() != param_names.size())
            {
                continue;
            }

            TypeSubstitutionMap substitutions;

            for (size_t i = 0; i < solid_types.size(); ++i)
            {
                substitutions[param_names[i]] = solid_types[i];
            }

            std::string mangled_name = cls->name + "_" + TypeSystem::mangle(solid_types);
            Symbol_ptr solid_symbol = solidify_template(cand, mangled_name, substitutions);
            return {solid_symbol->get_type(), solid_symbol};
        }
        else
        {
            auto deduced = deduce_class_template_arguments(cls, argument_types);

            if (deduced.has_value())
            {
                TypeVector deduced_types;

                for (const auto& name : cls->template_type->ordered_parameter_names)
                {
                    deduced_types.push_back(deduced->at(name));
                }

                std::string mangled_name = cls->name + "_" + TypeSystem::mangle(deduced_types);
                Symbol_ptr solid_symbol = solidify_template(cand, mangled_name, *deduced);

                return {solid_symbol->get_type(), solid_symbol};
            }
        }
    }

    Doctor::semantics().fatal("No viable constructor for: " + symbol->name);
}

} // namespace Wasp
