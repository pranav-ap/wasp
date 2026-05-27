#include "ASTFactory.h"
#include "Doctor.h"
#include "Expression.h"
#include "Objects.h"
#include "SemanticAnalyzer.h"
#include "Statement.h"
#include "SymbolScope.h"
#include "Workspace.h"

#include <ctime>

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

    Doctor::get().assert(
        type_system->implements_trait(current_scope, cond_type, "Truthy"),
        WaspStage::Semantics,
        "Condition type '" + cond_type->to_string() + "' is not truthy"
    );

    auto sugar = ASTFactory::create_method_call(expr.test, "is_truthy");
    expr.test = sugar;

    cond_type = visit(expr.test);
    type_system->expect_boolean_type(cond_type);

    Object_ptr then_type = visit(expr.true_expression);

    if (expr.alternative)
    {
        Object_ptr else_type = visit(expr.alternative);

        ObjectVector unique_types = type_system->remove_duplicates(
            current_scope,
            {then_type, else_type}
        );

        leave_scope();

        if (unique_types.size() == 1)
        {
            return unique_types[0];
        }

        return make_object(VariantType(unique_types));
    }

    Object_ptr none_type = make_object(NoneType());

    ObjectVector unique_types = type_system->remove_duplicates(
        current_scope,
        {then_type, none_type}
    );

    leave_scope();

    if (unique_types.size() == 1)
    {
        return unique_types[0];
    }

    return make_object(VariantType(unique_types));
}

Object_ptr SemanticAnalyzer::visit(ElseTernaryBranch& expr)
{
    Object_ptr type = visit(expr.expression);
    return type;
}

void SemanticAnalyzer::visit(IfBranch& statement)
{
    enter_scope(ScopeType::BRANCH);

    Object_ptr cond_type = visit(statement.test);

    Doctor::get().assert(
        type_system->implements_trait(current_scope, cond_type, "Truthy"),
        WaspStage::Semantics,
        "Condition type '" + cond_type->to_string() + "' is not truthy"
    );

    auto sugar = ASTFactory::create_method_call(statement.test, "is_truthy");
    statement.test = sugar;

    // resolves it to `@bool`
    cond_type = visit(statement.test);
    type_system->expect_boolean_type(cond_type);

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
    visit(statement.condition);
    enter_scope(ScopeType::LOOP);
    visit(statement.body);
    leave_scope();
}

void SemanticAnalyzer::visit(ForInLoop& loop_stmt)
{
    Object_ptr iterable_type = visit(loop_stmt.iterable);
    type_system->expect_iterable_type(current_scope, iterable_type);

    Object_ptr element_type = type_system->extract_iterable_element_type(
        current_scope,
        iterable_type
    );

    enter_scope(ScopeType::LOOP);

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
        "Loop control statement ('break', 'continue', 'redo') must be inside a loop."
    );
}

} // namespace Wasp
