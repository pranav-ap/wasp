#include "SemanticAnalyzer.h"
#include <stdexcept>

#ifndef ASSERT
#include <cassert>
#define ASSERT(condition, message) assert((condition) && message)
#endif

#define MAKE_OBJECT_VARIANT(x) std::make_shared<Object>(x)
#define FATAL(message) throw std::runtime_error(message)
#define NULL_CHECK(x) ASSERT(x != nullptr, "Oh shit! A nullptr")

template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...)->overloaded<Ts...>;

namespace Wasp {

void SemanticAnalyzer::visit(const Statement_ptr statement)
{
    if (!statement) return;

    std::visit(overloaded{
        [&](const std::monostate&) { return; },
        
        [&](std::shared_ptr<VariableDefinition>& s) { visit(*s); },
        [&](std::shared_ptr<ExpressionStatement>& s) { visit(*s); },
        [&](std::shared_ptr<FunctionDefinition>& s) { visit(*s); },
        [&](std::shared_ptr<AliasDefinition>& s) { visit(*s); },
        [&](std::shared_ptr<EnumDefinition>& s) { visit(*s); },
        [&](std::shared_ptr<ClassDefinition>& s) { visit(*s); },
        [&](std::shared_ptr<TraitDefinition>& s) { visit(*s); },
        [&](std::shared_ptr<ImplDefinition>& s) { visit(*s); },
        [&](std::shared_ptr<AnnotationDefinition>& s) { visit(*s); },
        [&](std::shared_ptr<IfBranch>& s) { visit(*s); },
        [&](std::shared_ptr<ElseBranch>& s) { visit(*s); },
        [&](std::shared_ptr<SimpleLoop>& s) { visit(*s); },
        [&](std::shared_ptr<ForInLoop>& s) { visit(*s); },
        [&](std::shared_ptr<LoopControl>& s) { visit(*s); },
        [&](std::shared_ptr<Return>& s) { visit(*s); },
        [&](std::shared_ptr<Pass>& s) { visit(*s); },
        
        [&](auto&) { FATAL("Unknown statement type visited."); }
    }, statement->data); // Assumes Statement has a variant named 'data'
}

void SemanticAnalyzer::visit(std::vector<Statement_ptr>& statements)
{
    for (const auto& stmt : statements) {
        visit(stmt);
    }
}

void SemanticAnalyzer::visit(VariableDefinition& statement)
{
    Object_ptr var_type;

    if (statement.type_annotation) {
        var_type = visit(statement.type_annotation);
        if (statement.initializer) {
            Object_ptr init_type = visit(statement.initializer);
            if (!type_system->assignable(current_scope, var_type, init_type)) {
                FATAL("Initializer type does not match variable annotation.");
            }
        }
    } else if (statement.initializer) {
        var_type = visit(statement.initializer);
    } else {
        var_type = type_system->type_pool->get_any_type();
    }

    // Add to scope
    auto symbol = std::make_shared<Symbol>(next_id++, statement.name, var_type, false, statement.is_mutable);
    current_scope->define(statement.name, symbol);
}

void SemanticAnalyzer::visit(ExpressionStatement& statement)
{
    visit(statement.expression);
}

void SemanticAnalyzer::visit(IfBranch& statement)
{
    Object_ptr condition_type = visit(statement.condition);
    type_system->expect_condition_type(current_scope, condition_type);

    enter_scope(ScopeType::BRANCH);
    visit(statement.body);
    leave_scope();
}

void SemanticAnalyzer::visit(Return& statement)
{
    if (statement.value) {
        Object_ptr return_type = visit(statement.value);
        // Note: You will need to check this return_type against the current 
        // enclosing function's expected return type.
    }
}

// Stubs for complex statements

void SemanticAnalyzer::visit(FunctionDefinition& statement) { FATAL("Not implemented"); }
void SemanticAnalyzer::visit(AliasDefinition& statement) { FATAL("Not implemented"); }
void SemanticAnalyzer::visit(EnumDefinition& statement) { FATAL("Not implemented"); }
void SemanticAnalyzer::visit(ClassDefinition& statement) { FATAL("Not implemented"); }
void SemanticAnalyzer::visit(TraitDefinition& statement) { FATAL("Not implemented"); }
void SemanticAnalyzer::visit(ImplDefinition& statement) { FATAL("Not implemented"); }
void SemanticAnalyzer::visit(AnnotationDefinition& statement) { FATAL("Not implemented"); }
void SemanticAnalyzer::visit(ElseBranch& statement) { FATAL("Not implemented"); }
void SemanticAnalyzer::visit(SimpleLoop& statement) { FATAL("Not implemented"); }
void SemanticAnalyzer::visit(ForInLoop& statement) { FATAL("Not implemented"); }
void SemanticAnalyzer::visit(LoopControl& statement) { FATAL("Not implemented"); }
void SemanticAnalyzer::visit(Pass& statement) { FATAL("Not implemented"); }
}
