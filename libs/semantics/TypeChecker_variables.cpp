#include "AST.h"
#include "Doctor.h"
#include "Expression.h"
#include "Symbol.h"
#include "SymbolScope.h"
#include "Type.h"
#include "TypeChecker.h"

#include <memory>
#include <string>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

Type_ptr TypeChecker::visit(Binding& binding)
{
    Doctor::semantics().assert(
        binding.lhs->is<Identifier>(),
        "Left-hand side of a binding must be an identifier"
    );

    auto& id = binding.lhs->as<Identifier>();

    current_scope->define(id.symbol);

    if (!binding.declared_type)
    {
        auto inferred_type = visit(binding.rhs);
        id.symbol->set_type(inferred_type);
        return inferred_type;
    }

    auto type = visit(binding.declared_type);
    id.symbol->set_type(type);

    return type;
}

Type_ptr TypeChecker::visit(Assignment& expr)
{
    if (expr.lhs->is<Identifier>())
    {
        return mutate_variable(expr.lhs, expr.rhs);
    }

    if (expr.lhs->is<MemberAccess>())
    {
        return mutate_member(expr.lhs, expr.rhs);
    }

    Doctor::semantics().fatal(
        "LHS of assignment must be an identifier or member access"
    );
}

void TypeChecker::validate_purity_constraints(
    Symbol_ptr target_symbol
) const
{
    if (target_symbol->closure_depth >= current_scope->closure_depth)
    {
        return;
    }

    auto scope = current_scope;

    while (scope && scope->closure_depth > target_symbol->closure_depth)
    {
        Doctor::semantics().assert(
            scope->type != ScopeType::PURE_FUNCTION &&
                scope->type != ScopeType::PURE_METHOD,
            "A pure function cannot mutate variables from an outer scope"
        );

        scope = scope->enclosing_scope;
    }
}

Type_ptr TypeChecker::mutate_variable(
    Expression_ptr identifier_expr,
    Expression_ptr assigned_expr
)
{
    auto& identifier_node = identifier_expr->as<Identifier>();
    std::string symbol_name = identifier_node.name;

    Symbol_ptr target_symbol = current_scope->lookup(symbol_name);

    Doctor::semantics().fatal_if_nullptr(
        target_symbol,
        "Cannot assign to undefined variable '" + symbol_name + "'"
    );

    Doctor::semantics().assert(
        target_symbol->is<VariableSymbol>(),
        "Cannot assign to non-variable symbol '" + symbol_name + "'"
    );

    auto& var_data = target_symbol->as<VariableSymbol>();

    Doctor::semantics().assert(
        var_data.is_mutable,
        "Cannot reassign immutable variable '" + symbol_name + "'"
    );

    validate_purity_constraints(target_symbol);

    identifier_node.symbol = target_symbol;
    Type_ptr assigned_type = visit(assigned_expr);

    Doctor::semantics().assert(
        type_system->assignable(
            current_scope,
            target_symbol->get_type(),
            assigned_type
        ),
        "Type mismatch in assignment to '" + symbol_name + "'"
    );

    return target_symbol->get_type();
}

Type_ptr TypeChecker::mutate_member(
    Expression_ptr lhs_expr,
    Expression_ptr rhs_expr
)
{
    auto& access = lhs_expr->as<MemberAccess>();
    Type_ptr expected_type = visit(access);

    auto symbol = access.object->as<Identifier>().symbol;

    if (symbol && symbol->is<VariableSymbol>())
    {
        Doctor::semantics().assert(
            symbol->as<VariableSymbol>().is_mutable,
            "Cannot reassign immutable shared member: " + symbol->name
        );
    }

    validate_purity_constraints(symbol);

    Type_ptr actual_type = visit(rhs_expr);

    Doctor::semantics().assert(
        type_system->assignable(current_scope, expected_type, actual_type),
        "Type mismatch in member assignment."
    );

    return expected_type;
}

} // namespace Wasp
