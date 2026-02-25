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
    if (!type_node) return type_system->type_pool->get_none_type();

    return std::visit(overloaded{
        [&](const std::monostate&) -> Object_ptr { 
            return type_system->type_pool->get_none_type(); 
        },
        
        [&](AnyTypeNode& node) -> Object_ptr { return visit(node); },
        [&](NoneTypeNode& node) -> Object_ptr { return visit(node); },

        [&](IntTypeNode& node) -> Object_ptr { return visit(node); },
        [&](FloatTypeNode& node) -> Object_ptr { return visit(node); },
        [&](StringTypeNode& node) -> Object_ptr { return visit(node); },
        [&](BoolTypeNode& node) -> Object_ptr { return visit(node); },
        
        [&](IntLiteralTypeNode& node) -> Object_ptr { return visit(node); },
        [&](FloatLiteralTypeNode& node) -> Object_ptr { return visit(node); },
        [&](StringLiteralTypeNode& node) -> Object_ptr { return visit(node); },
        [&](BoolLiteralTypeNode& node) -> Object_ptr { return visit(node); },
        
        [&](TypeIdentifierNode& node) -> Object_ptr { return visit(node); },

        [&](std::shared_ptr<ListTypeNode>& node) -> Object_ptr { return visit(*node); },
        [&](std::shared_ptr<TupleTypeNode>& node) -> Object_ptr { return visit(*node); },
        [&](std::shared_ptr<SetTypeNode>& node) -> Object_ptr { return visit(*node); },
        [&](std::shared_ptr<MapTypeNode>& node) -> Object_ptr { return visit(*node); },
        [&](std::shared_ptr<VariantTypeNode>& node) -> Object_ptr { return visit(*node); },
        [&](std::shared_ptr<FunctionTypeNode>& node) -> Object_ptr { return visit(*node); },
        [&](std::shared_ptr<RecordTypeNode>& node) -> Object_ptr { return visit(*node); },
        
        [&](auto&) -> Object_ptr { FATAL("Unknown type annotation visited."); return nullptr; }
        
    }, type_node->data);
}

ObjectVector SemanticAnalyzer::visit(std::vector<TypeAnnotation_ptr>& type_nodes)
{
    ObjectVector resolved_types;
    resolved_types.reserve(type_nodes.size());

    for (auto& node : type_nodes) 
    {
        resolved_types.push_back(visit(node));
    }
    
	return resolved_types;
}

// ----------------------------------------------------------------------------
// Primitive Types (Fetched directly from ConstantPool)
// ----------------------------------------------------------------------------

Object_ptr SemanticAnalyzer::visit(AnyTypeNode& expr)   { return type_system->type_pool->get_any_type(); }
Object_ptr SemanticAnalyzer::visit(NoneTypeNode& expr)  { return type_system->type_pool->get_none_type(); }

Object_ptr SemanticAnalyzer::visit(IntTypeNode& expr)   { return type_system->type_pool->get_int_type(); }
Object_ptr SemanticAnalyzer::visit(FloatTypeNode& expr) { return type_system->type_pool->get_float_type(); }
Object_ptr SemanticAnalyzer::visit(StringTypeNode& expr) { return type_system->type_pool->get_string_type(); }
Object_ptr SemanticAnalyzer::visit(BoolTypeNode& expr)  { return type_system->type_pool->get_boolean_type(); }

// ----------------------------------------------------------------------------
// Literal Types (Constructed fresh)
// ----------------------------------------------------------------------------

Object_ptr SemanticAnalyzer::visit(IntLiteralTypeNode& expr) { return MAKE_OBJECT_VARIANT(IntLiteralType(expr.value)); }
Object_ptr SemanticAnalyzer::visit(FloatLiteralTypeNode& expr) { return MAKE_OBJECT_VARIANT(FloatLiteralType(expr.value)); }
Object_ptr SemanticAnalyzer::visit(StringLiteralTypeNode& expr) { return MAKE_OBJECT_VARIANT(StringLiteralType(expr.value)); }
Object_ptr SemanticAnalyzer::visit(BoolLiteralTypeNode& expr) { return MAKE_OBJECT_VARIANT(BooleanLiteralType(expr.value)); }

// ----------------------------------------------------------------------------
// Type Identifiers 
// ----------------------------------------------------------------------------

Object_ptr SemanticAnalyzer::visit(TypeIdentifierNode& expr) 
{
    if (!current_scope->lookup_success(expr.name)) 
    {
        FATAL("Undefined type identifier: " + expr.name);
    }

    auto symbol = current_scope->lookup(expr.name);
    
    FATAL("Not Implemented yet! Type identifiers must refer to type definitions (classes, enums, aliases).");

    return symbol->type; 
}

// ----------------------------------------------------------------------------
// Composite Types (Recursive resolution)
// ----------------------------------------------------------------------------

Object_ptr SemanticAnalyzer::visit(ListTypeNode& expr) 
{
    Object_ptr element_type = visit(expr.element_type);
    return MAKE_OBJECT_VARIANT(ListType(element_type));
}

Object_ptr SemanticAnalyzer::visit(TupleTypeNode& expr) 
{
    ObjectVector element_types = visit(expr.element_types);
    return MAKE_OBJECT_VARIANT(TupleType(element_types));
}

Object_ptr SemanticAnalyzer::visit(SetTypeNode& expr) 
{
    Object_ptr element_type = visit(expr.element_type);
    return MAKE_OBJECT_VARIANT(SetType(element_type));
}

Object_ptr SemanticAnalyzer::visit(MapTypeNode& expr) 
{
    Object_ptr key_type = visit(expr.key_type);
    Object_ptr value_type = visit(expr.value_type);
    return MAKE_OBJECT_VARIANT(MapType(key_type, value_type));
}

Object_ptr SemanticAnalyzer::visit(VariantTypeNode& expr) 
{
    ObjectVector inner_types = visit(expr.types);
    return MAKE_OBJECT_VARIANT(VariantType(inner_types));
}

Object_ptr SemanticAnalyzer::visit(FunctionTypeNode& expr) 
{
    ObjectVector input_types = visit(expr.input_types);
    
    if (expr.return_type) {
        Object_ptr return_type = visit(expr.return_type);
        return MAKE_OBJECT_VARIANT(FunctionType(input_types, return_type));
    }
    
    return MAKE_OBJECT_VARIANT(FunctionType(input_types));
}

Object_ptr SemanticAnalyzer::visit(RecordTypeNode& expr) 
{
    std::map<std::string, Object_ptr> field_types;
    for (const auto& [field_name, field_type_node] : expr.fields) {
        field_types[field_name] = visit(field_type_node);
    }
    return MAKE_OBJECT_VARIANT(RecordType(field_types));
}
}
