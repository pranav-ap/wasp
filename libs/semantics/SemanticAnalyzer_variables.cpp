#include "AST.h"
#include "Doctor.h"
#include "Expression.h"
#include "Objects.h"
#include "SemanticAnalyzer.h"
#include "Statement.h"
#include "SymbolScope.h"
#include "Workspace.h"

#include <ctime>
#include <memory>
#include <string>
#include <variant>

#define MAKE_OBJECT_VARIANT(x) std::make_shared<Object>(x)

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

// ---------------------------------------------------------------------------
// Variable Definitions & Assignments
// --------------------------------------------------------------------------

void SemanticAnalyzer::visit(VariableDefinition& statement)
{
    define_variable(statement.expression, statement.is_mutable);
}

Object_ptr SemanticAnalyzer::visit(VariableDefinitionExpression& expr)
{
    return define_variable(expr.assignment, expr.is_mutable);
}

Object_ptr SemanticAnalyzer::visit(TypedAssignment& expr)
{
    Doctor::get().fatal(
        WaspStage::Semantics,
        "Internal Semantic Error: TypedAssignment visited directly."
    );
}

Object_ptr SemanticAnalyzer::visit(UntypedAssignment& expr)
{
    if (expr.lhs_expression->is<Identifier>())
    {
        return mutate_variable(expr.lhs_expression, expr.rhs_expression);
    }

    if (expr.lhs_expression->is<MemberAccess>())
    {
        return mutate_member(expr.lhs_expression, expr.rhs_expression);
    }

    Doctor::get().fatal(
        WaspStage::Semantics,
        "Left-hand side of assignment must be an Identifier or MemberAccess."
    );
}

Object_ptr SemanticAnalyzer::mutate_member(Expression_ptr lhs_expr, Expression_ptr rhs_expr)
{
    auto& mac = lhs_expr->as<MemberAccess>();

    Object_ptr expected_type = visit(mac);

    if (mac.member_index == -1)
    {
        auto symbol = mac.right->as<Identifier>().symbol;
        if (symbol && symbol->payload_is<VariableData>())
        {
            Doctor::get().assert(
                symbol->get_payload_as<VariableData>().is_mutable,
                WaspStage::Semantics,
                "Cannot reassign immutable shared member: " + symbol->name
            );
        }
    }

    Object_ptr actual_type = visit(rhs_expr);

    Doctor::get().assert(
        type_checker->assignable(current_scope, expected_type, actual_type),
        WaspStage::Semantics,
        "Type mismatch in member assignment."
    );

    return expected_type;
}

void SemanticAnalyzer::validate_purity_constraints(Symbol_ptr target_symbol) const
{
    if (target_symbol->closure_depth >= current_scope->get_closure_depth())
    {
        return;
    }

    auto scope = current_scope;

    while (scope != nullptr && scope->get_closure_depth() > target_symbol->closure_depth)
    {
        if (scope->get_type() == ScopeType::PURE_FUNCTION ||
            scope->get_type() == ScopeType::PURE_METHOD)
        {
            Doctor::get().fatal(
                WaspStage::Semantics,
                "Pure functions cannot mutate outer state variable '" + target_symbol->name + "'"
            );
        }

        scope = scope->get_enclosing();
    }
}

Object_ptr SemanticAnalyzer::mutate_variable(
    Expression_ptr identifier_expr,
    Expression_ptr assigned_expr
)
{
    Doctor::get().assert(
        identifier_expr->is<Identifier>(),
        WaspStage::Semantics,
        "Left-hand side of assignment must be an Identifier."
    );

    auto& identifier_node = identifier_expr->as<Identifier>();
    std::string symbol_name = identifier_node.name;

    Symbol_ptr target_symbol = current_scope->lookup(symbol_name);

    Doctor::get().fatal_if_nullptr(
        target_symbol,
        WaspStage::Semantics,
        "Cannot assign to undefined variable '" + symbol_name + "'"
    );

    Doctor::get().assert(
        target_symbol->payload_is<VariableData>(),
        WaspStage::Semantics,
        "Cannot assign to non-variable symbol '" + symbol_name + "'"
    );

    auto& var_data = target_symbol->get_payload_as<VariableData>();

    Doctor::get().assert(
        var_data.is_mutable,
        WaspStage::Semantics,
        "Cannot reassign immutable variable '" + symbol_name + "'"
    );

    validate_purity_constraints(target_symbol);

    identifier_node.symbol = target_symbol;

    Object_ptr assigned_type = visit(assigned_expr);

    Doctor::get().assert(
        type_checker->assignable(current_scope, target_symbol->get_type(), assigned_type),
        WaspStage::Semantics,
        "Type mismatch in assignment to '" + symbol_name + "'"
    );

    return target_symbol->get_type();
}

Object_ptr SemanticAnalyzer::define_variable(Expression_ptr assignment_node, bool is_mutable)
{
    Expression_ptr identifier_expr = nullptr;
    Expression_ptr rhs_expr = nullptr;
    Object_ptr declared_type = nullptr;

    std::visit(
        overloaded{
            [&](UntypedAssignment& assign)
            {
                identifier_expr = assign.lhs_expression;
                rhs_expr = assign.rhs_expression;
            },
            [&](TypedAssignment& assign)
            {
                identifier_expr = assign.lhs_expression;
                rhs_expr = assign.rhs_expression;
                declared_type = visit(assign.type_node);
            },
            [](auto&)
            {
                Doctor::get().fatal(WaspStage::Semantics, "Invalid variable definition expression");
            }
        },
        assignment_node->data
    );

    Doctor::get().assert(
        identifier_expr->is<Identifier>(),
        WaspStage::Semantics,
        "Left-hand side of definition must be an Identifier."
    );

    std::string symbol_name = identifier_expr->as<Identifier>().name;

    Object_ptr initializer_type = visit(rhs_expr);
    Object_ptr resolved_type = initializer_type;

    if (declared_type)
    {
        Doctor::get().assert(
            type_checker->assignable(current_scope, declared_type, initializer_type),
            WaspStage::Semantics,
            "Type mismatch in variable definition for " + symbol_name
        );
        resolved_type = declared_type;
    }
    else
    {
        // literal type widening (e.g. int literal -> int type)

        if (initializer_type->is<IntLiteralType>())
            resolved_type = workspace->pool->get_int_type();
        else if (initializer_type->is<FloatLiteralType>())
            resolved_type = workspace->pool->get_float_type();
        else if (initializer_type->is<StringLiteralType>())
            resolved_type = workspace->pool->get_string_type();
        else if (initializer_type->is<BooleanLiteralType>())
            resolved_type = workspace->pool->get_boolean_type();
    }

    if (Symbol_ptr hoisted_symbol = current_scope->lookup(symbol_name))
    {
        Doctor::get().assert(
            hoisted_symbol->get_type() == nullptr,
            WaspStage::Semantics,
            "Variable '" + symbol_name + "' is hoisted but already has a type!"
        );

        hoisted_symbol->set_type(resolved_type);
        identifier_expr->as<Identifier>().symbol = hoisted_symbol;

        return hoisted_symbol->get_type();
    }

    auto local_symbol = SymbolFactory::create_variable(
        symbol_name,
        resolved_type,
        is_mutable,
        current_scope->get_closure_depth(),
        current_scope->get_lexical_depth()
    );

    current_scope->define(local_symbol);
    identifier_expr->as<Identifier>().symbol = local_symbol;

    return local_symbol->get_type();
}

} // namespace Wasp
