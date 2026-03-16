#include "SemanticAnalyzer.h"
#include "Doctor.h"
#include "Expression.h"
#include "Objects.h"
#include "Statement.h"
#include "Symbol.h"
#include "SymbolScope.h"

#include <ctime>
#include <memory>
#include <string>

#define MAKE_OBJECT_VARIANT(x) std::make_shared<Object>(x)

template <class... Ts> struct overloaded : Ts... {
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp {
Object_ptr SemanticAnalyzer::visit(IfTernaryBranch& expr) {
    enter_scope(ScopeType::BRANCH);

    Object_ptr cond_type = visit(expr.test);
    type_checker->expect_condition_type(current_scope, cond_type);

    Object_ptr then_type = visit(expr.true_expression);
    leave_scope();

    if (expr.alternative) {
        Object_ptr else_type = visit(expr.alternative);
        ObjectVector unique_types =
            type_checker->remove_duplicates(current_scope, {then_type, else_type});

        if (unique_types.size() == 1) {
            return unique_types[0];
        }

        return MAKE_OBJECT_VARIANT(VariantType(unique_types));
    }

    Object_ptr none_type = MAKE_OBJECT_VARIANT(NoneType());
    ObjectVector unique_types =
        type_checker->remove_duplicates(current_scope, {then_type, none_type});

    if (unique_types.size() == 1)
        return unique_types[0];

    return MAKE_OBJECT_VARIANT(VariantType(unique_types));
}

void SemanticAnalyzer::visit(IfBranch& statement) {
    enter_scope(ScopeType::BRANCH);

    Object_ptr cond_type = visit(statement.test);
    type_checker->expect_condition_type(current_scope, cond_type);

    visit(statement.body);
    leave_scope();

    if (statement.alternative) {
        visit(*statement.alternative);
    }
}

Object_ptr SemanticAnalyzer::visit(ElseTernaryBranch& expr) {
    enter_scope(ScopeType::BRANCH);
    Object_ptr type = visit(expr.expression);
    leave_scope();

    return type;
}

void SemanticAnalyzer::visit(ElseBranch& statement) {
    enter_scope(ScopeType::BRANCH);
    visit(statement.body);
    leave_scope();
}

void SemanticAnalyzer::visit(Pass& statement) {}

// ---------------------------------------------------------------------------
// Loops
// ---------------------------------------------------------------------------

void SemanticAnalyzer::visit(SimpleLoop& statement) {
    visit(statement.condition);
    enter_scope(ScopeType::LOOP);
    visit(statement.body);
    leave_scope();
}

void SemanticAnalyzer::visit(ForInLoop& loop_stmt) {
    Object_ptr iterable_type = visit(loop_stmt.iterable_expression);
    type_checker->expect_iterable_type(current_scope, iterable_type);

    Object_ptr element_type = MAKE_OBJECT_VARIANT(AnyType());

    enter_scope(ScopeType::LOOP);

    Doctor::get().assert(
        loop_stmt.lhs->is<Identifier>(),
        WaspStage::Semantics,
        "For-in loop variable must be an identifier."
    );

    auto& identifier_node = loop_stmt.lhs->as<Identifier>();
    std::string symbol_name = identifier_node.name;

    auto loop_variable_symbol = SymbolFactory::create_variable(
        symbol_name,
        element_type,
        loop_stmt.lhs_is_mutable,
        false, // is_captured
        current_scope->get_closure_depth(),
        current_scope->get_lexical_depth()
    );

    current_scope->define(loop_variable_symbol);
    identifier_node.symbol = loop_variable_symbol;

    visit(loop_stmt.body);

    leave_scope();
}

void SemanticAnalyzer::visit(LoopControl& statement) {
    SymbolScope_ptr scope = current_scope;

    Doctor::get().assert(
        scope->enclosed_in(ScopeType::LOOP),
        WaspStage::Semantics,
        "Loop control statement ('break', 'continue', 'redo') must be inside a loop."
    );
}

} // namespace Wasp
