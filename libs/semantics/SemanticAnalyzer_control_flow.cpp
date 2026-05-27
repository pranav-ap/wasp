#include "Doctor.h"
#include "Expression.h"
#include "Objects.h"
#include "SemanticAnalyzer.h"
#include "Statement.h"
#include "SymbolScope.h"
#include "Workspace.h"

#include <ctime>
#include <memory>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

Object_ptr SemanticAnalyzer::visit(IfTernaryBranch& expr)
{
    enter_scope(ScopeType::BRANCH);

    Object_ptr cond_type = visit(expr.test);
    cond_type = cond_type->unwrap_completely();

    // Check condition is boolean (after Truthy trait would be desugared to
    // is_truthy() call)
    Doctor::get().assert(
        type_system->is_boolean_type(cond_type),
        WaspStage::Semantics,
        "Condition type '" + cond_type->to_string() + "' is not boolean"
    );

    Object_ptr then_type = visit(expr.true_expression);
    then_type = then_type->unwrap_completely();

    if (expr.alternative)
    {
        Object_ptr else_type = visit(expr.alternative);
        else_type = else_type->unwrap_completely();

        ObjectVector unique_types = type_system->remove_duplicates(
            current_scope,
            {then_type, else_type}
        );

        leave_scope();

        if (unique_types.size() == 1)
        {
            return unique_types[0];
        }

        return make_object(std::make_shared<VariantType>(unique_types));
    }

    Object_ptr none_type = workspace->pool->get_none_type();

    ObjectVector unique_types = type_system->remove_duplicates(
        current_scope,
        {then_type, none_type}
    );

    leave_scope();

    if (unique_types.size() == 1)
    {
        return unique_types[0];
    }

    return make_object(std::make_shared<VariantType>(unique_types));
}

Object_ptr SemanticAnalyzer::visit(ElseTernaryBranch& expr)
{
    Object_ptr type = visit(expr.expression);
    return type->unwrap_completely();
}

void SemanticAnalyzer::visit(IfBranch& statement)
{
    enter_scope(ScopeType::BRANCH);

    Object_ptr cond_type = visit(statement.test);
    cond_type = cond_type->unwrap_completely();

    Doctor::get().assert(
        type_system->is_boolean_type(cond_type),
        WaspStage::Semantics,
        "Condition type '" + cond_type->to_string() + "' is not boolean"
    );

    visit(statement.body);
    leave_scope();

    if (statement.alternative)
    {
        visit(*statement.alternative);
    }
}

void SemanticAnalyzer::visit(ElseBranch& statement)
{
    enter_scope(ScopeType::BRANCH);
    visit(statement.body);
    leave_scope();
}

// ---------------------------------------------------------------------------
// Loops
// ---------------------------------------------------------------------------

void SemanticAnalyzer::visit(SimpleLoop& statement)
{
    // Visit condition first
    Object_ptr cond_type = visit(statement.condition);
    cond_type = cond_type->unwrap_completely();

    // Condition should be boolean (after desugaring)
    Doctor::get().assert(
        type_system->is_boolean_type(cond_type),
        WaspStage::Semantics,
        "Loop condition type '" + cond_type->to_string() + "' is not boolean"
    );

    enter_scope(ScopeType::LOOP);
    visit(statement.body);
    leave_scope();
}

void SemanticAnalyzer::visit(ForInLoop& loop_stmt)
{
    Object_ptr iterable_type = visit(loop_stmt.iterable);
    iterable_type = iterable_type->unwrap_completely();

    Doctor::get().assert(
        type_system->is_iterable_type(current_scope, iterable_type),
        WaspStage::Semantics,
        "Type '" + iterable_type->to_string() + "' is not iterable"
    );

    Object_ptr element_type = type_system->extract_iterable_element_type(
        current_scope,
        iterable_type
    );
    element_type = element_type->unwrap_completely();

    enter_scope(ScopeType::LOOP);

    // Create iterator symbol (hidden variable for desugared iteration)
    auto iterator_symbol = SymbolFactory::create_dummy(
        ".iter",
        iterable_type,
        current_scope->get_closure_depth(),
        current_scope->get_lexical_depth()
    );

    current_scope->define(iterator_symbol);
    loop_stmt.iterator_symbol = iterator_symbol;

    Doctor::get().assert(
        loop_stmt.lhs->is<Identifier>(),
        WaspStage::Semantics,
        "For-in loop variable must be an identifier."
    );

    auto& identifier_node = loop_stmt.lhs->as<Identifier>();

    auto loop_variable_symbol = SymbolFactory::create_variable(
        identifier_node.name,
        element_type,
        loop_stmt.lhs_is_mutable,
        current_scope->get_closure_depth(),
        current_scope->get_lexical_depth()
    );

    current_scope->define(loop_variable_symbol);
    identifier_node.symbol = loop_variable_symbol;

    visit(loop_stmt.body);

    leave_scope();
}

void SemanticAnalyzer::visit(LoopControl& statement)
{
    SymbolScope_ptr scope = current_scope;

    Doctor::get().assert(
        scope->enclosed_in(ScopeType::LOOP),
        WaspStage::Semantics,
        "Loop control statement ('break', 'continue', 'redo') must be inside a "
        "loop."
    );
}

} // namespace Wasp
