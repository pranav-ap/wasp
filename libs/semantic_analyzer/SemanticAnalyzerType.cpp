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

// ============================================================================
// Type Visitors 
// ============================================================================

Object_ptr SemanticAnalyzer::visit(const TypeAnnotation_ptr type_node)
{
    return nullptr;
}

ObjectVector SemanticAnalyzer::visit(std::vector<TypeAnnotation_ptr>& type_nodes)
{
    return {};
}

// ----------------------------------------------------------------------------
// Primitive Types
// ----------------------------------------------------------------------------

Object_ptr SemanticAnalyzer::visit(AnyTypeNode& expr)   { return nullptr; }
Object_ptr SemanticAnalyzer::visit(NoneTypeNode& expr)  { return nullptr; }

Object_ptr SemanticAnalyzer::visit(IntTypeNode& expr)   { return nullptr; }
Object_ptr SemanticAnalyzer::visit(FloatTypeNode& expr) { return nullptr; }
Object_ptr SemanticAnalyzer::visit(StringTypeNode& expr) { return nullptr; }
Object_ptr SemanticAnalyzer::visit(BoolTypeNode& expr)  { return nullptr; }

// ----------------------------------------------------------------------------
// Literal Types
// ----------------------------------------------------------------------------

Object_ptr SemanticAnalyzer::visit(IntLiteralTypeNode& expr) { return nullptr; }
Object_ptr SemanticAnalyzer::visit(FloatLiteralTypeNode& expr) { return nullptr; }
Object_ptr SemanticAnalyzer::visit(StringLiteralTypeNode& expr) { return nullptr; }
Object_ptr SemanticAnalyzer::visit(BoolLiteralTypeNode& expr) { return nullptr; }

// ----------------------------------------------------------------------------
// Type Identifiers 
// ----------------------------------------------------------------------------

Object_ptr SemanticAnalyzer::visit(TypeIdentifierNode& expr) 
{
    return nullptr; 
}

// ----------------------------------------------------------------------------
// Composite Types
// ----------------------------------------------------------------------------

Object_ptr SemanticAnalyzer::visit(ListTypeNode& expr) 
{
    return nullptr;
}

Object_ptr SemanticAnalyzer::visit(TupleTypeNode& expr) 
{
    return nullptr;
}

Object_ptr SemanticAnalyzer::visit(SetTypeNode& expr) 
{
    return nullptr;
}

Object_ptr SemanticAnalyzer::visit(MapTypeNode& expr) 
{
    return nullptr;
}

Object_ptr SemanticAnalyzer::visit(VariantTypeNode& expr) 
{
    return nullptr;
}

Object_ptr SemanticAnalyzer::visit(FunctionTypeNode& expr) 
{
    return nullptr;
}

Object_ptr SemanticAnalyzer::visit(RecordTypeNode& expr) 
{
    return nullptr;
}
}