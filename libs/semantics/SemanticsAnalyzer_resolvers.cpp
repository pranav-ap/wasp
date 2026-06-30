#include "AST.h"
#include "Doctor.h"
#include "Expression.h"
#include "Symbol.h"
#include "SymbolFactory.h"
#include "SymbolScope.h"
#include "SemanticsAnalyzer.h"
#include "Type.h"
#include "TypeSystem.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <variant>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

namespace
{

void validate_purity_constraints(
    SymbolScope_ptr scope,
    Symbol_ptr target_symbol
)
{
    if (target_symbol->closure_depth >= scope->closure_depth)
    {
        return;
    }

    while (scope && scope->closure_depth > target_symbol->closure_depth)
    {
        bool inside_pure = scope->type == ScopeType::PURE_FUNCTION ||
                           scope->type == ScopeType::PURE_METHOD;

        Doctor::semantics().check(
            inside_pure,
            "A pure function cannot mutate variables from an outer scope"
        );

        scope = scope->enclosing_scope;
    }
}

StringVector unfurl_member_access(const MemberAccess& expr)
{
    StringVector path = {expr.member->as<Identifier>().name};
    Expression_ptr current = expr.owner;

    while (current && current->is<MemberAccess>())
    {
        auto& nested_ma = current->as<MemberAccess>();
        if (!nested_ma.member->is<Identifier>())
        {
            break;
        }

        path.push_back(nested_ma.member->as<Identifier>().name);
        current = nested_ma.owner;
    }

    if (current && current->is<Identifier>())
    {
        path.push_back(current->as<Identifier>().name);
    }

    std::reverse(path.begin(), path.end());
    return path;
}

std::optional<Type_ptr> try_resolve_as_enum(
    SymbolScope_ptr scope,
    MemberAccess& ma
)
{
    StringVector path = unfurl_member_access(ma);

    if (path.size() < 2)
    {
        return std::nullopt;
    }

    Symbol_ptr base_sym = scope->lookup(path.front());

    if (!base_sym || !base_sym->get_type())
    {
        return std::nullopt;
    }

    Type_ptr base_type = base_sym->get_type();

    if (base_type->is<TypeAlias_ptr>())
    {
        base_type = base_type->unwrap_alias();
    }

    if (!base_type->is<EnumType_ptr>())
    {
        return std::nullopt;
    }

    auto enum_type = base_type->as<EnumType_ptr>();
    int value = enum_type->get_value(path);

    Doctor::semantics().check(value != -1, "Enum member not found.");

    return make_shared_type<EnumMemberType>(enum_type, value);
}

Type_ptr resolve_member_access(
    MemberAccess& ma,
    Type_ptr target_type,
    const std::string& member_name,
    SymbolScope_ptr scope,
    TypeSystem_ptr type_system
)
{
    return std::visit(
        overloaded{
            [&](ModuleType_ptr type) -> Type_ptr
            {
                ma.member_index = type->get_member_index(member_name);
                return type->get_member(member_name);
            },

            [&](ClassType_ptr type) -> Type_ptr
            {
                Doctor::semantics().check(
                    type->contains_member(member_name),
                    type->name + " has no member named " + member_name
                );

                if (type->is_field(member_name))
                {
                    ma.member_index = type->fields->get_index(member_name);
                    return type->fields->get_type(member_name);
                }

                ma.member_index = type->methods->get_index(member_name);
                return make_type(type->methods->get_type(member_name));
            },

            [&](TraitType_ptr type) -> Type_ptr
            {
                Doctor::semantics().check(
                    type->contains_member(member_name),
                    type->name + " has no member named " + member_name
                );

                ma.member_index = type->methods->get_index(member_name);
                return make_type(type->methods->get_type(member_name));
            },

            [&](GenericType_ptr type) -> Type_ptr
            {
                Doctor::semantics().fatal_if_nullptr(
                    type->constraint_type
                );

                return resolve_member_access(
                    ma,
                    type->constraint_type,
                    member_name,
                    scope,
                    type_system
                );
            },

            [&](IntersectionType_ptr intersect) -> Type_ptr
            {
                TypeVector result_types;

                for (const auto& t : intersect->types)
                {
                    result_types.push_back(resolve_member_access(
                        ma,
                        t,
                        member_name,
                        scope,
                        type_system
                    ));
                }

                auto resolved_type = TypeSystem::unify(scope, result_types);

                return resolved_type;
            },

            [&](auto&) -> Type_ptr
            {
                Doctor::semantics().fatal(
                    "Invalid LHS for a member access : " + member_name
                );
            }
        },
        target_type->data
    );
}

} // namespace

// ===============================================================================
// Binding
// ===============================================================================

Type_ptr SemanticsAnalyzer::visit(Binding& binding)
{
    Doctor::semantics().check(
        binding.lhs->is<Identifier>(),
        "Left-hand side of a binding must be an identifier"
    );

    auto& id = binding.lhs->as<Identifier>();

    id.symbol = SymbolFactory::create_variable(
        id.name,
        nullptr,
        binding.is_mutable,
        current_scope->closure_depth,
        current_scope->lexical_depth
    );

    current_scope->define(id.symbol);

    Type_ptr inferred_type = visit(binding.rhs);

    if (!binding.declared_type)
    {
        id.symbol->set_type(inferred_type);
        return inferred_type;
    }

    Type_ptr declared_type = visit(binding.declared_type);
    id.symbol->set_type(declared_type);

    Doctor::semantics().check(
        TypeSystem::equal(current_scope, declared_type, inferred_type),
        "Declared type and inferred type do not match"
    );

    return inferred_type;
}

// ===============================================================================
// Assignment
// ===============================================================================

Type_ptr SemanticsAnalyzer::visit(Assignment& expr)
{
    if (expr.lhs->is<Identifier>())
    {
        return mutate_variable(expr.lhs, expr.rhs);
    }

    if (expr.lhs->is<MemberAccess>())
    {
        return mutate_member(expr.lhs, expr.rhs);
    }

    Doctor::semantics().fatal("Unexpected in Assignment LHS");
}

Type_ptr SemanticsAnalyzer::mutate_variable(
    Expression_ptr expr,
    Expression_ptr assigned_expr
)
{
    auto& identifier = expr->as<Identifier>();
    std::string symbol_name = identifier.name;

    identifier.symbol = current_scope->lookup_variable(symbol_name);

    const VariableSymbol& variable_symbol = identifier.symbol
                                                ->as<VariableSymbol>();

    Doctor::semantics().check(
        variable_symbol.is_mutable,
        "Cannot mutate constant '" + symbol_name + "'"
    );

    validate_purity_constraints(current_scope, identifier.symbol);

    Type_ptr assigned_type = visit(assigned_expr);
    Type_ptr expected_type = identifier.symbol->get_type();

    Doctor::semantics().check(
        TypeSystem::equal(current_scope, expected_type, assigned_type),
        "Type mismatch in assignment to '" + symbol_name + "'"
    );

    return expected_type;
}

Type_ptr SemanticsAnalyzer::mutate_member(Expression_ptr lhs_expr, Expression_ptr rhs_expr)
{
    auto& access = lhs_expr->as<MemberAccess>();
    Type_ptr expected_type = visit(access);

    Type_ptr actual_type = visit(rhs_expr);

    Doctor::semantics().check(
        TypeSystem::equal(current_scope, expected_type, actual_type),
        "Type mismatch in member assignment to '" + access.member->as<Identifier>().name + "'"
    );

    return expected_type;
}

// ===============================================================================
// Identifier
// ===============================================================================

Type_ptr SemanticsAnalyzer::visit(Identifier& expr)
{
    auto symbol = current_scope->lookup_required_and_resolve(expr.name);
    expr.symbol = symbol;
    return symbol->get_type();
}

// ===============================================================================
// Member Access
// ===============================================================================

Type_ptr SemanticsAnalyzer::visit(MemberAccess& access)
{
    Doctor::semantics().check(
        access.member->is<Identifier>(),
        "RHS of member access must be an identifier."
    );

    std::optional<Type_ptr> enum_type_object = try_resolve_as_enum(
        current_scope,
        access
    );

    if (enum_type_object.has_value())
    {
        return enum_type_object.value();
    }

    Type_ptr left_type = visit(access.owner);

    return resolve_member_access(
        access,
        left_type,
        access.member->as<Identifier>().name,
        current_scope,
        type_system
    );
}

} // namespace Wasp
