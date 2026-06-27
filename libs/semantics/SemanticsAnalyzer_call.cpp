#include "AST.h"
#include "ASTCloner.h"
#include "Doctor.h"
#include "Expression.h"
#include "SemanticsAnalyzer.h"
#include "Solidifier.h"
#include "Statement.h"
#include "Symbol.h"
#include "SymbolFactory.h"
#include "SymbolScope.h"
#include "Type.h"
#include "TypeSystem.h"

#include <algorithm>
#include <cstddef>
#include <map>
#include <optional>
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

namespace
{

FunctionCandidateVector filter_by_arity(const SymbolVector& candidates, size_t arity)
{
    FunctionCandidateVector result;

    for (const auto& candidate : candidates)
    {
        Type_ptr type = candidate->get_type();

        Doctor::semantics().check(type->is<FunctionType_ptr>(), "Expected a FunctionType for candidate");

        FunctionType_ptr func_type = candidate->get_type()->as<FunctionType_ptr>();

        if (func_type->parameter_types.size() == arity)
        {
            result.push_back({candidate, -1, func_type});
        }
    }

    return result;
}

std::pair<FunctionCandidateVector, FunctionCandidateVector> separate_solid_and_template(
    const FunctionCandidateVector& candidates
)
{
    FunctionCandidateVector solid;
    FunctionCandidateVector templated;

    for (const auto& c : candidates)
    {
        if (!c.function_type->template_type->empty())
        {
            templated.push_back(c);
        }
        else
        {
            solid.push_back(c);
        }
    }

    return {solid, templated};
}

} // namespace

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
                return visit(call, ma, solid_types, argument_types);
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
    const TypeVector& solid_types,
    const TypeVector& argument_types
)
{
    Symbol_ptr symbol = current_scope->lookup_overload(identifier.name);
    SymbolVector& candidate_symbols = symbol->as<OverloadSymbol>().overloads;

    FunctionCandidateVector by_arity = filter_by_arity(candidate_symbols, argument_types.size());
    auto [solid_candidates, template_candidates] = separate_solid_and_template(by_arity);

    std::optional<std::pair<Symbol_ptr, int>> solid_result = try_resolve_solid(
        symbol->name,
        solid_candidates,
        argument_types
    );

    if (solid_result.has_value())
    {
        auto [function_symbol, overload_index] = solid_result.value();

        call.overload_index = overload_index;

        identifier.symbol = function_symbol;
        identifier.must_be_captured = function_symbol->should_be_captured(current_scope->closure_depth);

        std::string mangled_name = identifier.name + "_" + TypeSystem::mangle(argument_types);
        function_symbol->mangled_name = mangled_name;

        FunctionType_ptr function_type = function_symbol->get_type()->as<FunctionType_ptr>();
        return function_type->return_type;
    }

    std::optional<std::tuple<Symbol_ptr, int, TypeSubstitutionMap>> template_result = try_resolve_template(
        symbol->name,
        template_candidates,
        solid_types,
        argument_types
    );

    if (template_result.has_value())
    {
        auto [template_function_symbol, overload_index, substitutions] = template_result.value();
        call.overload_index = overload_index;

        std::string mangled_name = identifier.name + "_" + TypeSystem::mangle(solid_types);

        identifier.name = mangled_name;

        Symbol_ptr solid_function_symbol = current_scope->lookup(mangled_name);

        Type_ptr template_function_symbol_type = template_function_symbol->get_type();
        FunctionType_ptr template_function_type = template_function_symbol_type->as<FunctionType_ptr>();

        Type_ptr solid_function_symbol_type = Solidifier::get().substitute_type(
            template_function_symbol_type,
            substitutions
        );

        if (!solid_function_symbol)
        {
            solid_function_symbol = SymbolFactory::create_type(
                mangled_name,
                solid_function_symbol_type,
                current_scope->closure_depth,
                current_scope->lexical_depth
            );

            current_scope->define(solid_function_symbol);
        }

        solid_function_symbol->mangled_name = mangled_name;

        auto [template_function_definition_stmt, definition_scope] = get_tree(template_function_symbol);

        Statement_ptr template_function_definition_stmt_copy = ASTCloner::get().clone(
            template_function_definition_stmt
        );

        Statement_ptr solid_ast = Solidifier::get().visit(
            template_function_definition_stmt_copy,
            substitutions
        );

        solid_ast->as<FunctionDefinition>().symbol = solid_function_symbol;

        add_tree(solid_function_symbol, solid_ast, current_scope);

        identifier.symbol = solid_function_symbol;
        identifier.must_be_captured = solid_function_symbol->should_be_captured(current_scope->closure_depth);

        FunctionType_ptr solid_function_type = solid_function_symbol_type->as<FunctionType_ptr>();
        return solid_function_type->return_type;
    }

    Doctor::semantics().fatal("No viable candidates for function: " + symbol->name);
}

std::optional<std::pair<Symbol_ptr, int>> SemanticsAnalyzer::try_resolve_solid(
    const std::string& name,
    const FunctionCandidateVector& candidates,
    const TypeVector& argument_types
) const
{
    FunctionCandidateVector viable;

    for (const auto& c : candidates)
    {
        bool candidate_is_viable = true;

        for (size_t i = 0; i < argument_types.size(); ++i)
        {
            bool arg_is_assignable = type_system->assignable(
                current_scope,
                c.function_type->parameter_types[i],
                argument_types[i]
            );

            if (!arg_is_assignable)
            {
                candidate_is_viable = false;
                break;
            }
        }

        if (candidate_is_viable)
        {
            viable.push_back(c);
        }
    }

    if (viable.empty())
    {
        return std::nullopt;
    }

    if (viable.size() == 1)
    {
        return std::make_pair(viable[0].symbol, viable[0].index);
    }

    auto best = get_best_candidate(viable, argument_types);
    return std::make_pair(best.symbol, best.index);
}

std::optional<std::tuple<Symbol_ptr, int, TypeSubstitutionMap>> SemanticsAnalyzer::try_resolve_template(
    const std::string& name,
    const FunctionCandidateVector& candidates,
    const TypeVector& solid_types,
    const TypeVector& argument_types
) const
{
    if (candidates.empty())
    {
        return std::nullopt;
    }

    FunctionCandidateVector viable;
    TypeSubstitutionMap latest_substitutions;

    for (const auto& c : candidates)
    {
        auto [ok, subs] = is_assignable_template_function(c.function_type, solid_types, argument_types);

        if (ok)
        {
            viable.push_back(c);
            latest_substitutions = subs;
        }
    }

    if (viable.empty())
    {
        return std::nullopt;
    }

    if (viable.size() == 1)
    {
        return std::make_tuple(viable[0].symbol, viable[0].index, latest_substitutions);
    }

    // Multiple template candidates - check for ambiguity
    Doctor::semantics().fatal("Ambiguous template function call: " + name);
}

FunctionCandidate SemanticsAnalyzer::get_best_candidate(
    const FunctionCandidateVector& candidates,
    const TypeVector& argument_types
) const
{
    // Score candidates: prefer more specific matches
    std::vector<std::pair<FunctionCandidate, int>> scored;

    for (const auto& c : candidates)
    {
        int score = 0;

        // Exact matches score higher
        for (size_t i = 0; i < argument_types.size(); ++i)
        {
            if (type_system->equal(current_scope, c.function_type->parameter_types[i], argument_types[i]))
            {
                score += 10;
            }
        }

        scored.push_back({c, score});
    }

    // Sort by score descending
    std::sort(
        scored.begin(),
        scored.end(),
        [](const auto& a, const auto& b)
        {
            return a.second > b.second;
        }
    );

    // Check for tie
    if (scored.size() > 1 && scored[0].second == scored[1].second)
    {
        Doctor::semantics().fatal("Ambiguous function call");
    }

    return scored[0].first;
}

Type_ptr SemanticsAnalyzer::visit(
    Call& call,
    MemberAccess& access,
    const TypeVector& solid_types,
    const TypeVector& argument_types
)
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

            [&](ModuleType_ptr module_type) -> Type_ptr
            {
                call.owner_kind = Call::OwnerKind::MODULE;
                call.owner_name = module_type->name;

                return visit(call, access, solid_types, argument_types, module_type);
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
    MethodCandidateVector viable;

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

std::pair<bool, TypeSubstitutionMap> SemanticsAnalyzer::is_assignable_template_function(
    FunctionType_ptr function_type,
    const TypeVector& solid_types,
    const TypeVector& argument_types
) const
{
    TypeSubstitutionMap substitutions;

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
        substituted_params.push_back(Solidifier::get().substitute_type(param_type, substitutions));
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

Type_ptr SemanticsAnalyzer::visit(
    Call& call,
    MemberAccess& access,
    const TypeVector& solid_types,
    const TypeVector& argument_types,
    ModuleType_ptr module_type
)
{
    std::string function_name = access.member->as<Identifier>().name;

    const Type_ptr& function_type = module_type->get_member(function_name);

    Doctor::semantics().check(
        function_type->is<FunctionType_ptr>(),
        "Module member '" + function_name + "' is not a function"
    );

    return visit(call, access, solid_types, argument_types);
}

} // namespace Wasp
