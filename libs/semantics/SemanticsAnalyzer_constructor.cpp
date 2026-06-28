#include "AST.h"
#include "ASTCloner.h"
#include "Doctor.h"
#include "Expression.h"
#include "SemanticsAnalyzer.h"
#include "Solidifier.h"
#include "Statement.h"
#include "Symbol.h"
#include "SymbolFactory.h"
#include "Type.h"
#include "TypeSystem.h"

#include <algorithm>
#include <cstddef>
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

ClassCandidateVector filter_by_arity(const SymbolVector& candidates, size_t arity)
{
    ClassCandidateVector result;

    for (size_t i = 0; i < candidates.size(); ++i)
    {
        const auto& candidate = candidates[i];
        Type_ptr type = candidate->get_type();

        Doctor::semantics().check(type->is<ClassType_ptr>(), "Expected a ClassType for candidate");

        ClassType_ptr class_type = candidate->get_type()->as<ClassType_ptr>();

        if (class_type->fields->ordered_keys.size() == arity)
        {
            result.push_back({candidate, class_type, static_cast<int>(i)});
        }
    }

    return result;
}

std::pair<ClassCandidateVector, ClassCandidateVector> separate_solid_and_template(
    const ClassCandidateVector& candidates
)
{
    ClassCandidateVector solid;
    ClassCandidateVector templated;

    for (const auto& c : candidates)
    {
        if (!c.class_type->template_type->empty())
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

Type_ptr SemanticsAnalyzer::visit(Constructor& cons)
{
    TypeVector solid_types = visit(cons.angular_nodes);
    TypeVector argument_types = visit(cons.arguments);

    return std::visit(
        overloaded{
            [&](Identifier& id) -> Type_ptr
            {
                Symbol_ptr symbol = current_scope->lookup_required_and_resolve(id.name);
                return resolve_constructor_from_symbol(symbol, solid_types, argument_types);
            },
            [&](MemberAccess& ma) -> Type_ptr
            {
                Type_ptr owner_type = visit(ma.owner);
                owner_type = owner_type->unwrap_alias();

                if (owner_type->is<ModuleType_ptr>())
                {
                    ModuleType_ptr mod_type = owner_type->as<ModuleType_ptr>();
                    Module_ptr mod = workspace->get_module(mod_type->absolute_filepath);
                    Doctor::semantics().fatal_if_nullptr(mod);

                    Doctor::semantics().check(
                        ma.member->is<Identifier>(),
                        "Module member must be an identifier"
                    );

                    std::string member_name = ma.member->as<Identifier>().name;
                    int member_index = mod_type->get_member_index(member_name);
                    Symbol_ptr member_symbol = mod->exported_symbols[member_index];
                    Doctor::semantics().fatal_if_nullptr(member_symbol);

                    return resolve_constructor_from_symbol(member_symbol, solid_types, argument_types);
                }
                else
                {
                    Doctor::semantics().fatal("Nested constructor calls are not supported yet");
                }
            },
            [&](auto&) -> Type_ptr
            {
                Doctor::semantics().fatal("Invalid constructible");
            }
        },
        cons.constructible->data
    );
}

Type_ptr SemanticsAnalyzer::resolve_constructor_from_symbol(
    Symbol_ptr symbol,
    const TypeVector& solid_types,
    const TypeVector& argument_types
)
{
    // symbol should be an OverloadSymbol or a single ClassType symbol
    SymbolVector candidates;
    if (symbol->is<OverloadSymbol>())
    {
        candidates = symbol->as<OverloadSymbol>().overloads;
    }
    else
    {
        candidates.push_back(symbol);
    }

    ClassCandidateVector by_arity = filter_by_arity(candidates, argument_types.size());
    auto [solid_candidates, template_candidates] = separate_solid_and_template(by_arity);

    // Try solid
    std::optional<std::pair<Symbol_ptr, int>> solid_result = try_resolve_solid(
        symbol->name,
        solid_candidates,
        argument_types
    );

    if (solid_result.has_value())
    {
        auto [class_symbol, overload_index] = solid_result.value();
        // store overload_index if needed
        return class_symbol->get_type();
    }

    std::optional<std::tuple<Symbol_ptr, int, TypeSubstitutionMap>> template_result = try_resolve_template(
        symbol->name,
        template_candidates,
        solid_types,
        argument_types
    );

    if (template_result.has_value())
    {
        auto [template_class_symbol, overload_index, substitutions] = template_result.value();

        std::string mangled_name = symbol->name + "_" + TypeSystem::mangle(solid_types);

        Symbol_ptr solid_symbol = solidify_template(template_class_symbol, mangled_name, substitutions);

        return solid_symbol->get_type();
    }

    Doctor::semantics().fatal("No viable constructor for: " + symbol->name);
}

Type_ptr SemanticsAnalyzer::visit(
    Constructor& cons,
    Identifier& identifier,
    const TypeVector& solid_types,
    const TypeVector& argument_types
)
{
    Symbol_ptr symbol = current_scope->lookup_required_and_resolve(identifier.name);
    SymbolVector candidate_symbols = {symbol};

    ClassCandidateVector by_arity = filter_by_arity(candidate_symbols, argument_types.size());
    auto [solid_candidates, template_candidates] = separate_solid_and_template(by_arity);

    std::optional<std::pair<Symbol_ptr, int>> solid_result = try_resolve_solid(
        symbol->name,
        solid_candidates,
        argument_types
    );

    if (solid_result.has_value())
    {
        auto [class_symbol, overload_index] = solid_result.value();

        identifier.symbol = class_symbol;
        identifier.must_be_captured = class_symbol->should_be_captured(current_scope->closure_depth);

        std::string mangled_name = identifier.name + "_" + TypeSystem::mangle(argument_types);
        class_symbol->mangled_name = mangled_name;

        return class_symbol->get_type();
    }

    std::optional<std::tuple<Symbol_ptr, int, TypeSubstitutionMap>> template_result = try_resolve_template(
        symbol->name,
        template_candidates,
        solid_types,
        argument_types
    );

    if (template_result.has_value())
    {
        auto [template_class_symbol, overload_index, substitutions] = template_result.value();

        std::string mangled_name = identifier.name + "_" + TypeSystem::mangle(solid_types);

        identifier.name = mangled_name;

        Symbol_ptr solid_oops_symbol = current_scope->lookup(mangled_name);

        Type_ptr template_class_symbol_type = template_class_symbol->get_type();
        ClassType_ptr template_class_type = template_class_symbol_type->as<ClassType_ptr>();

        Type_ptr solid_class_symbol_type = Solidifier::get().substitute_type(
            template_class_symbol_type,
            substitutions
        );

        if (!solid_oops_symbol)
        {
            solid_oops_symbol = SymbolFactory::create_type(
                mangled_name,
                solid_class_symbol_type,
                current_scope->closure_depth,
                current_scope->lexical_depth
            );

            current_scope->define(solid_oops_symbol);
        }

        solid_oops_symbol->mangled_name = mangled_name;

        auto [template_class_definition_stmt, definition_scope] = get_tree(template_class_symbol);

        Statement_ptr template_class_definition_stmt_copy = ASTCloner::get().clone(
            template_class_definition_stmt
        );

        Statement_ptr solid_ast = Solidifier::get().visit(template_class_definition_stmt_copy, substitutions);

        if (solid_ast->is<ClassDefinition>())
        {
            solid_ast->as<ClassDefinition>().symbol = solid_oops_symbol;
        }
        else if (solid_ast->is<TraitDefinition>())
        {
            solid_ast->as<TraitDefinition>().symbol = solid_oops_symbol;
        }
        else
        {
            Doctor::semantics().fatal("Expected a ClassDefinition or TraitDefinition");
        }

        add_tree(solid_oops_symbol, solid_ast, current_scope);

        identifier.symbol = solid_oops_symbol;
        identifier.must_be_captured = solid_oops_symbol->should_be_captured(current_scope->closure_depth);

        return solid_class_symbol_type;
    }

    Doctor::semantics().fatal(identifier.name + " is not a constructible type");
}

std::optional<std::pair<Symbol_ptr, int>> SemanticsAnalyzer::try_resolve_solid(
    const std::string& name,
    const ClassCandidateVector& candidates,
    const TypeVector& argument_types
) const
{
    ClassCandidateVector viable;

    for (const auto& c : candidates)
    {
        bool candidate_is_viable = true;

        for (size_t i = 0; i < argument_types.size(); ++i)
        {
            bool arg_is_assignable = type_system->assignable(
                current_scope,
                c.class_type->fields->get_type(i),
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

ClassCandidate SemanticsAnalyzer::get_best_candidate(
    const ClassCandidateVector& candidates,
    const TypeVector& argument_types
) const
{
    // Score candidates: prefer more specific matches
    std::vector<std::pair<ClassCandidate, int>> scored;

    for (const auto& c : candidates)
    {
        int score = 0;

        // Exact matches score higher
        for (size_t i = 0; i < argument_types.size(); ++i)
        {
            if (type_system->equal(current_scope, c.class_type->fields->get_type(i), argument_types[i]))
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

std::optional<std::tuple<Symbol_ptr, int, TypeSubstitutionMap>> SemanticsAnalyzer::try_resolve_template(
    const std::string& name,
    const ClassCandidateVector& candidates,
    const TypeVector& solid_types,
    const TypeVector& argument_types
) const
{
    if (candidates.empty())
    {
        return std::nullopt;
    }

    ClassCandidateVector viable;
    TypeSubstitutionMap latest_substitutions;

    for (const auto& c : candidates)
    {
        auto [ok, subs] = is_constructible_template_class(c.class_type, solid_types, argument_types);

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

std::pair<bool, TypeSubstitutionMap> SemanticsAnalyzer::is_constructible_template_class(
    ClassType_ptr class_type,
    const TypeVector& solid_types,
    const TypeVector& argument_types
) const
{
    TypeSubstitutionMap substitutions;

    const StringVector& template_params = class_type->template_type->ordered_parameter_names;

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

    for (const Type_ptr& param_type : class_type->fields->get_ordered_types())
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

} // namespace Wasp
