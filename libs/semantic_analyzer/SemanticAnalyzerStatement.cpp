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
    // Do nothing
}

void SemanticAnalyzer::visit(std::vector<Statement_ptr>& statements)
{
    // Do nothing
}

void SemanticAnalyzer::visit(VariableDefinition& statement)
{
    // Do nothing
}

void SemanticAnalyzer::visit(ExpressionStatement& statement)
{
    // Do nothing
}

void SemanticAnalyzer::visit(IfBranch& statement)
{
    // Do nothing
}

void SemanticAnalyzer::visit(Return& statement)
{
    // Do nothing
}

// Stubs for complex statements

void SemanticAnalyzer::visit(FunctionDefinition& statement) { }
void SemanticAnalyzer::visit(AliasDefinition& statement) { }
void SemanticAnalyzer::visit(EnumDefinition& statement) { }
void SemanticAnalyzer::visit(ClassDefinition& statement) { }
void SemanticAnalyzer::visit(TraitDefinition& statement) { }
void SemanticAnalyzer::visit(ImplDefinition& statement) { }
void SemanticAnalyzer::visit(AnnotationDefinition& statement) { }
void SemanticAnalyzer::visit(ElseBranch& statement) { }
void SemanticAnalyzer::visit(SimpleLoop& statement) { }
void SemanticAnalyzer::visit(ForInLoop& statement) { }
void SemanticAnalyzer::visit(LoopControl& statement) { }
void SemanticAnalyzer::visit(Pass& statement) { }

} // namespace Wasp