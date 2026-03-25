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

Object_ptr SemanticAnalyzer::visit(UntypedAssignment& expr)
{
    return mutate_variable(expr.lhs_expression, expr.rhs_expression);
}

Object_ptr SemanticAnalyzer::visit(TypedAssignment& expr)
{
    Doctor::get().fatal(
        WaspStage::Semantics,
        "Internal Semantic Error: TypedAssignment visited directly.");

    return nullptr;
}

Object_ptr SemanticAnalyzer::define_variable(Expression_ptr assignment_node, bool is_mutable)
{
    Expression_ptr identifier_expr = nullptr;
    Expression_ptr initializer_expr = nullptr;
    Object_ptr declared_type = nullptr;

    std::visit(
        overloaded{
            [&](UntypedAssignment& assign)
            {
                identifier_expr = assign.lhs_expression;
                initializer_expr = assign.rhs_expression;
            },
            [&](TypedAssignment& assign)
            {
                identifier_expr = assign.lhs_expression;
                initializer_expr = assign.rhs_expression;
                declared_type = visit(assign.type_node);
            },
            [](auto&)
            {
                Doctor::get().fatal(WaspStage::Semantics, "Invalid variable definition expression");
            }},
        assignment_node->data);

    Doctor::get().assert(
        identifier_expr->is<Identifier>(),
        WaspStage::Semantics,
        "Left-hand side of definition must be an Identifier.");

    std::string symbol_name = identifier_expr->as<Identifier>().name;

    // Resolve RHS Type
    Object_ptr initializer_type = visit(initializer_expr);
    Object_ptr resolved_type = initializer_type;

    // Check whether it is assignable
    if (declared_type)
    {
        Doctor::get().assert(
            type_checker->assignable(current_scope, declared_type, initializer_type),
            WaspStage::Semantics,
            "Type mismatch in variable definition for " + symbol_name);

        resolved_type = declared_type;
    }

    // Hoister Usage

    if (Symbol_ptr hoisted_symbol = current_scope->lookup(symbol_name))
    {
        // If it exists, it must be a hoisted global waiting for its type.
        Doctor::get().assert(
            hoisted_symbol->get_type() == nullptr,
            WaspStage::Semantics,
            "Variable '" + symbol_name + "' is hoisted but already has a type!"
        );

        hoisted_symbol->set_type(resolved_type);
        identifier_expr->as<Identifier>().symbol = hoisted_symbol;

        return hoisted_symbol->get_type();
    }

    // New Local Variable

    auto local_symbol = SymbolFactory::create_variable(
        symbol_name,
        resolved_type,
        is_mutable,
        current_scope->get_closure_depth(),
        current_scope->get_lexical_depth());

    current_scope->define(local_symbol);
    identifier_expr->as<Identifier>().symbol = local_symbol;

    return local_symbol->get_type();
}

Object_ptr SemanticAnalyzer::mutate_variable(
    Expression_ptr identifier_expr,
    Expression_ptr assigned_expr)
{
    Doctor::get().assert(
        identifier_expr->is<Identifier>(),
        WaspStage::Semantics,
        "Left-hand side of assignment must be an Identifier.");

    auto& identifier_node = identifier_expr->as<Identifier>();
    std::string symbol_name = identifier_node.name;

    Symbol_ptr target_symbol = current_scope->lookup(symbol_name);

    Doctor::get().fatal_if_nullptr(
        target_symbol,
        WaspStage::Semantics,
        "Cannot assign to undefined variable '" + symbol_name);

    Doctor::get().assert(
        target_symbol->payload_is<VariableData>(),
        WaspStage::Semantics,
        "Cannot assign to non-variable symbol '" + symbol_name);

    auto& var_data = target_symbol->get_payload_as<VariableData>();

    Doctor::get().assert(
        var_data.is_mutable,
        WaspStage::Semantics,
        "Cannot reassign immutable variable '" + symbol_name);

    identifier_node.symbol = target_symbol;

    // Type Checking
    Object_ptr assigned_type = visit(assigned_expr);

    Doctor::get().assert(
        type_checker->assignable(current_scope, target_symbol->get_type(), assigned_type),
        WaspStage::Semantics,
        "Type mismatch in assignment to '" + symbol_name);

    return target_symbol->get_type();
}

} // namespace Wasp
