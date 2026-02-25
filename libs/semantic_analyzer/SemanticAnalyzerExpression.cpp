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

Object_ptr SemanticAnalyzer::visit(const Expression_ptr expr)
{
    return nullptr;
}

ObjectVector SemanticAnalyzer::visit(ExpressionVector expressions)
{
    return {};
}

// Primitives
Object_ptr SemanticAnalyzer::visit(int expr) { return nullptr; }
Object_ptr SemanticAnalyzer::visit(double expr) { return nullptr; }
Object_ptr SemanticAnalyzer::visit(std::string expr) { return nullptr; }
Object_ptr SemanticAnalyzer::visit(bool expr) { return nullptr; }

// Identifiers
Object_ptr SemanticAnalyzer::visit(Identifier& expr)
{
    return nullptr;
}

// Operators
Object_ptr SemanticAnalyzer::visit(Prefix& expr)
{
    return nullptr;
}

Object_ptr SemanticAnalyzer::visit(Infix& expr)
{
    return nullptr;
}

Object_ptr SemanticAnalyzer::visit(Postfix& expr)
{
    return nullptr;
}

// Collections

Object_ptr SemanticAnalyzer::visit(ListLiteral& expr)
{
    return nullptr;
}

Object_ptr SemanticAnalyzer::visit(TupleLiteral& expr)
{
    return nullptr;
}

// Assignments
Object_ptr SemanticAnalyzer::visit(UntypedAssignment& expr)
{
    return nullptr; 
}

Object_ptr SemanticAnalyzer::visit(TypedAssignment& expr)
{
    return nullptr;
}

// Stubs for complex expressions
Object_ptr SemanticAnalyzer::visit(MapLiteral& expr) { return nullptr; }
Object_ptr SemanticAnalyzer::visit(SetLiteral& expr) { return nullptr; }
Object_ptr SemanticAnalyzer::visit(RangeLiteral& expr) { return nullptr; }
Object_ptr SemanticAnalyzer::visit(TypePattern& expr) { return nullptr; }
Object_ptr SemanticAnalyzer::visit(VariableDefinitionExpression& expr) { return nullptr; }
Object_ptr SemanticAnalyzer::visit(IfTernaryBranch& expr) { return nullptr; }
Object_ptr SemanticAnalyzer::visit(ElseTernaryBranch& expr) { return nullptr; }
Object_ptr SemanticAnalyzer::visit(Call& expr) { return nullptr; }
Object_ptr SemanticAnalyzer::visit(DotLiteral& expr) { return nullptr; }
Object_ptr SemanticAnalyzer::visit(DotDotLiteral& expr) { return nullptr; }
Object_ptr SemanticAnalyzer::visit(DotDotDotLiteral& expr) { return nullptr; }

Object_ptr SemanticAnalyzer::infer_chain_member_type(Object_ptr lhs, Expression_ptr expr, bool null_check) { return nullptr; }
}

