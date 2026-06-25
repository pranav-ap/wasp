#include "AST.h"
#include "Doctor.h"
#include "Expression.h"
#include "SemanticsAnalyzer.h"
#include "Statement.h"
#include "Symbol.h"
#include "SymbolScope.h"
#include "Type.h"

#include <cstddef>
#include <map>
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

Type_ptr SemanticsAnalyzer::visit(Call& call)
{
    TypeVector argument_types = visit(call.arguments);
    TypeVector solid_types = visit(call.angular_nodes);

    return std::visit(
        overloaded{
            [&](Identifier& id)
            {
                return visit(call, id, solid_types, argument_types);
            },
            [&](MemberAccess& ma)
            {
                Doctor::semantics().check(solid_types.empty(), "Generics on method calls are not allowed.");
                return visit(call, ma, argument_types);
            },
            [&](auto&) -> Type_ptr
            {
                Doctor::semantics().fatal("Invalid callable");
            }
        },
        call.callee->data
    );
}

Type_ptr SemanticsAnalyzer::visit(
    Call& call,
    Identifier& identifier,
    const TypeVector& soild_types,
    const TypeVector& argument_types
)
{
    Symbol_ptr symbol = current_scope->lookup_overload(identifier.name);

    bool should_capture = symbol->should_be_captured(current_scope->closure_depth);

    if (should_capture)
    {
        identifier.must_be_captured = true;
    }

    SymbolVector& candidates = symbol->as<OverloadSymbol>().overloads;

    auto [function_symbol, raw_index, substitutions] = resolve_function(
        symbol->name,
        candidates,
        soild_types,
        argument_types
    );

    identifier.symbol = function_symbol;

    FunctionType_ptr function_type = function_symbol->get_type()->as<FunctionType_ptr>();
    call.overload_index = raw_index;

    if (!function_type->template_type->empty())
    {
        std::string mangled_name = symbol->name + "_" + type_system->mangle_name(soild_types);

        auto [template_ast, definition_scope] = get_tree(function_symbol);

        Doctor::semantics().fatal_if_nullptr(
            template_ast,
            "Template function AST not found for " + mangled_name
        );

        Doctor::semantics().check(
            template_ast->is<FunctionDefinition>(),
            "Expected a FunctionDefinition for the template function"
        );
    }

    return function_type->return_type;
}

std::tuple<Symbol_ptr, int, std::map<std::string, Type_ptr>> SemanticsAnalyzer::resolve_function(
    const std::string& name,
    const SymbolVector& candidates,
    const TypeVector& soild_types,
    const TypeVector& argument_types
) const
{
    struct Candidate
    {
        Symbol_ptr symbol;
        int index;
        FunctionType_ptr function_type;
    };

    std::vector<Candidate> viable;

    for (size_t i = 0; i < candidates.size(); ++i)
    {
        const Symbol_ptr& function_symbol = candidates[i];
        const TypeSymbol& function_type_symbol = function_symbol->as<TypeSymbol>();
        const FunctionType_ptr function_type = function_type_symbol.type->as<FunctionType_ptr>();

        // Skip if arity doesn't match
        if (function_type->parameter_types.size() != argument_types.size())
        {
            continue;
        }

        // Check if arguments are assignable to parameters
        bool found_any_unassignable = false;

        for (size_t j = 0; j < argument_types.size(); ++j)
        {
            found_any_unassignable = !type_system->assignable(
                current_scope,
                function_type->parameter_types[j],
                argument_types[j]
            );

            if (found_any_unassignable)
            {
                break;
            }
        }

        if (!found_any_unassignable)
        {
            viable.push_back({function_symbol, static_cast<int>(i), function_type});
        }
    }

    Doctor::semantics().check(!viable.empty(), "No viable candidates for function " + name);

    std::map<std::string, Type_ptr> substitutions = {};

    // Only one candidate. Return it.
    if (viable.size() == 1)
    {
        return {viable[0].symbol, viable[0].index, substitutions};
    }

    // Possibilities
    // function ambiguity / one template function / template function ambiguity

    // check if there is one template function assignable to the arguments
    std::vector<Candidate> template_candidates;

    for (const Candidate& candidate : viable)
    {
        if (candidate.function_type->template_type)
        {
            template_candidates.push_back(candidate);
        }
    }

    Doctor::semantics().check(
        template_candidates.size() >= 1,
        "Expected at least one template function candidate for function " + name
    );

    bool found_valid_template_function = false;
    viable = {};

    for (const Candidate& candidate : template_candidates)
    {
        auto [yes, subs] = is_assignable_template_function(
            candidate.function_type,
            soild_types,
            argument_types
        );

        if (yes)
        {
            if (found_valid_template_function)
            {
                Doctor::semantics().fatal("Ambiguous call to template function '" + name + "'");
            }

            found_valid_template_function = true;
            viable.push_back(candidate);
            substitutions = subs;
        }
    }

    Doctor::semantics().check(!viable.empty(), "No viable candidates for template function " + name);

    Doctor::semantics().check(viable.size() == 1, "Ambiguous call to template function '" + name + "'");

    return {viable[0].symbol, viable[0].index, substitutions};
}

std::pair<bool, std::map<std::string, Type_ptr>> SemanticsAnalyzer::is_assignable_template_function(
    FunctionType_ptr function_type,
    const TypeVector& solid_types,
    const TypeVector& argument_types
) const
{
    // A template function is assignable if:
    // 1. It has a template_type
    // 2. The number of solid types matches the template parameters
    // 3. After substituting, the arguments are assignable to parameters

    // Build substitution map from solid types
    std::map<std::string, Type_ptr> substitutions;

    if (!function_type->template_type)
    {
        return {false, substitutions};
    }

    const StringVector& template_params = function_type->template_type->ordered_parameter_names;

    // Check if solid_types count matches template parameters
    if (solid_types.size() != template_params.size())
    {
        return {false, substitutions};
    }

    for (size_t i = 0; i < solid_types.size(); ++i)
    {
        substitutions[template_params[i]] = solid_types[i];
    }

    // Substitute parameter types
    TypeVector substituted_params;

    for (const Type_ptr& param_type : function_type->parameter_types)
    {
        substituted_params.push_back(substitute_type(param_type, substitutions));
    }

    // Check if arguments are assignable to substituted parameters
    for (size_t i = 0; i < argument_types.size(); ++i)
    {
        bool is_assignable = type_system->assignable(current_scope, substituted_params[i], argument_types[i]);

        if (!is_assignable)
        {
            return {false, substitutions};
        }
    }

    return {true, substitutions};
}

Type_ptr SemanticsAnalyzer::substitute_type(
    Type_ptr type,
    const std::map<std::string, Type_ptr>& substitutions
) const
{
    if (!type)
    {
        return nullptr;
    }

    if (type->is<GenericType_ptr>())
    {
        auto generic = type->as<GenericType_ptr>();
        auto it = substitutions.find(generic->name);
        if (it != substitutions.end())
        {
            return it->second;
        }
        return type;
    }

    // Handle composite types
    if (type->is<ListType_ptr>())
    {
        auto list = type->as<ListType_ptr>();
        auto new_element = substitute_type(list->element_type, substitutions);
        return make_shared_type<ListType>(new_element);
    }

    if (type->is<SetType_ptr>())
    {
        auto set = type->as<SetType_ptr>();
        auto new_element = substitute_type(set->element_type, substitutions);
        return make_shared_type<SetType>(new_element);
    }

    if (type->is<MapType_ptr>())
    {
        auto map = type->as<MapType_ptr>();
        auto new_key = substitute_type(map->key_type, substitutions);
        auto new_value = substitute_type(map->value_type, substitutions);
        return make_shared_type<MapType>(new_key, new_value);
    }

    if (type->is<TupleType_ptr>())
    {
        auto tuple = type->as<TupleType_ptr>();
        TypeVector new_elements;
        for (const auto& elem : tuple->element_types)
        {
            new_elements.push_back(substitute_type(elem, substitutions));
        }
        return make_shared_type<TupleType>(new_elements);
    }

    return type;
}

Type_ptr SemanticsAnalyzer::visit(Call& call, MemberAccess& access, const TypeVector& argument_types)
{
    Type_ptr left_type = visit(access.owner);
    left_type = left_type->unwrap_alias();

    return std::visit(
        overloaded{
            [&](ClassType_ptr class_type) -> Type_ptr
            {
                call.owner_kind = Call::OwnerKind::CLASS;
                call.owner_name = class_type->name;

                return visit(call, access, argument_types, class_type);
            },

            [&](TraitType_ptr trait_type) -> Type_ptr
            {
                call.owner_kind = Call::OwnerKind::TRAIT;
                call.owner_name = trait_type->name;

                return visit(call, access, argument_types, trait_type);
            },

            [&](PrimitiveType_ptr primitive_type) -> Type_ptr
            {
                call.owner_kind = Call::OwnerKind::PRIMITIVE;
                call.owner_name = primitive_type->name;

                return visit(call, access, argument_types, primitive_type);
            },

            [&](auto&) -> Type_ptr
            {
                Doctor::semantics().fatal("Invalid member call LHS");
            }
        },
        left_type->data
    );
}

Type_ptr SemanticsAnalyzer::visit(
    Call& call,
    MemberAccess& ma,
    const TypeVector& argument_types,
    const OopsType_ptr owner_type
)
{
    std::string method_name = ma.member->as<Identifier>().name;

    const MethodTypeVector& method_types = owner_type->methods->get_type(method_name);

    auto [method_type, overload_index] = resolve_method(method_types, argument_types);

    ma.member_index = owner_type->methods->get_index(method_name);
    call.overload_index = overload_index;

    return method_type->return_type;
}

std::tuple<MethodType_ptr, int> SemanticsAnalyzer::resolve_method(
    const MethodTypeVector& method_types,
    const TypeVector& argument_types
) const
{
    struct Candidate
    {
        MethodType_ptr method_type;
        int index;
    };

    std::vector<Candidate> viable;

    for (size_t i = 0; i < method_types.size(); ++i)
    {
        auto& method_type = method_types[i];

        // Skip if arity doesn't match
        if (method_type->parameter_types.size() != argument_types.size())
        {
            continue;
        }

        // Check if arguments are assignable to parameters
        bool all_assignable = true;

        for (size_t j = 0; j < argument_types.size(); ++j)
        {
            bool is_assignable = type_system->assignable(
                current_scope,
                method_type->parameter_types[j],
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
            viable.push_back({method_type, static_cast<int>(i)});
        }
    }

    Doctor::semantics().check(!viable.empty(), "No viable candidates for function call");

    // Only one candidate.
    // Return it.
    if (viable.size() == 1)
    {
        return {viable[0].method_type, viable[0].index};
    }

    Doctor::semantics().fatal("Ambiguous method call");
}

} // namespace Wasp
