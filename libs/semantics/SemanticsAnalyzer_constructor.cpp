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

                auto [result_type, resolved_symbol] = resolve_constructor(
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

                auto [result_type, resolved_symbol] = resolve_constructor(
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

std::pair<Type_ptr, Symbol_ptr> SemanticsAnalyzer::resolve_constructor(
    Symbol_ptr symbol,
    const TypeVector& solid_types,
    const TypeVector& argument_types
)
{
    Doctor::semantics().check(symbol->is<TypeSymbol>(), "Expected a TypeSymbol for constructor resolution");

    Type_ptr type = symbol->get_type();
    Doctor::semantics().check(type->is<ClassType_ptr>(), "Expected a ClassType for constructor");

    ClassType_ptr cls = type->as<ClassType_ptr>();

    if (cls->template_type->empty())
    {
        validate_solid_constructor(argument_types, cls);
        return {type, symbol};
    }

    if (!solid_types.empty())
    {
        return resolve_explicit_class_construction(symbol, solid_types, argument_types, cls);
    }

    return resolve_implicit_class_construction(symbol, argument_types, cls);
}

void SemanticsAnalyzer::validate_solid_constructor(const TypeVector& argument_types, ClassType_ptr cls)
{
    Doctor::semantics().check(
        cls->fields->ordered_keys.size() == argument_types.size(),
        "Constructor argument count mismatch for class " + cls->name
    );

    bool all_assignable = true;

    for (size_t i = 0; i < argument_types.size(); ++i)
    {
        if (!TypeSystem::assignable(current_scope, cls->fields->get_type(i), argument_types[i]))
        {
            all_assignable = false;
            break;
        }
    }

    Doctor::semantics().check(all_assignable, "Constructor argument type mismatch for class " + cls->name);
}

std::pair<Type_ptr, Symbol_ptr> SemanticsAnalyzer::resolve_explicit_class_construction(
    Symbol_ptr template_symbol,
    const TypeVector& solid_types,
    const TypeVector& argument_types,
    ClassType_ptr cls
)
{
    const auto& param_names = cls->template_type->ordered_parameter_names;

    Doctor::semantics().check(
        solid_types.size() == param_names.size(),
        "Template argument count mismatch for class " + cls->name
    );

    TypeSubstitutionMap substitutions;

    for (size_t i = 0; i < solid_types.size(); ++i)
    {
        substitutions[param_names[i]] = solid_types[i];
    }

    std::string mangled_name = cls->name + "_" + TypeSystem::mangle(solid_types);
    Symbol_ptr solid_symbol = solidify_template(template_symbol, mangled_name, substitutions);

    return {solid_symbol->get_type(), solid_symbol};
}

std::pair<Type_ptr, Symbol_ptr> SemanticsAnalyzer::resolve_implicit_class_construction(
    Symbol_ptr template_symbol,
    const TypeVector& argument_types,
    ClassType_ptr cls
)
{
    std::optional<TypeSubstitutionMap> solid_types_map = TypeSystem::infer_solid_types(
        current_scope,
        cls->fields->get_ordered_types(),
        cls->template_type->ordered_parameter_names,
        argument_types
    );

    Doctor::semantics().check(
        solid_types_map.has_value(),
        "Cannot deduce template arguments for class " + cls->name
    );

    TypeVector solid_types;

    for (const std::string& name : cls->template_type->ordered_parameter_names)
    {
        solid_types.push_back(solid_types_map->at(name));
    }

    std::string mangled_name = cls->name + "_" + TypeSystem::mangle(solid_types);
    Symbol_ptr solid_symbol = solidify_template(template_symbol, mangled_name, *solid_types_map);

    return {solid_symbol->get_type(), solid_symbol};
}

} // namespace Wasp
