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
    if (!expr) return type_system->type_pool->get_none_type();

    return std::visit(overloaded{
        [&](const std::monostate&) -> Object_ptr { return type_system->type_pool->get_none_type(); },
        
        // Pass value types directly
        [&](int& e) -> Object_ptr { return visit(e); },
        [&](double& e) -> Object_ptr { return visit(e); },
        [&](std::string& e) -> Object_ptr { return visit(e); },
        [&](bool& e) -> Object_ptr { return visit(e); },
        [&](Identifier& e) -> Object_ptr { return visit(e); },

        // Dereference pointer types
        [&](std::shared_ptr<DotLiteral>& e) -> Object_ptr { return visit(*e); },
        [&](std::shared_ptr<DotDotLiteral>& e) -> Object_ptr { return visit(*e); },
        [&](std::shared_ptr<DotDotDotLiteral>& e) -> Object_ptr { return visit(*e); },
        [&](std::shared_ptr<Prefix>& e) -> Object_ptr { return visit(*e); },
        [&](std::shared_ptr<Infix>& e) -> Object_ptr { return visit(*e); },
        [&](std::shared_ptr<Postfix>& e) -> Object_ptr { return visit(*e); },
        [&](std::shared_ptr<ListLiteral>& e) -> Object_ptr { return visit(*e); },
        [&](std::shared_ptr<TupleLiteral>& e) -> Object_ptr { return visit(*e); },
        [&](std::shared_ptr<MapLiteral>& e) -> Object_ptr { return visit(*e); },
        [&](std::shared_ptr<SetLiteral>& e) -> Object_ptr { return visit(*e); },
        [&](std::shared_ptr<RangeLiteral>& e) -> Object_ptr { return visit(*e); },
        [&](std::shared_ptr<TypePattern>& e) -> Object_ptr { return visit(*e); },
        [&](std::shared_ptr<VariableDefinitionExpression>& e) -> Object_ptr { return visit(*e); },
        [&](std::shared_ptr<UntypedAssignment>& e) -> Object_ptr { return visit(*e); },
        [&](std::shared_ptr<TypedAssignment>& e) -> Object_ptr { return visit(*e); },
        [&](std::shared_ptr<IfTernaryBranch>& e) -> Object_ptr { return visit(*e); },
        [&](std::shared_ptr<ElseTernaryBranch>& e) -> Object_ptr { return visit(*e); },
        [&](std::shared_ptr<Call>& e) -> Object_ptr { return visit(*e); },
        
        // Fallback
        [&](auto&) -> Object_ptr { FATAL("Unknown expression type visited."); return nullptr; }
    }, expr->data);
}

ObjectVector SemanticAnalyzer::visit(ExpressionVector expressions)
{
    ObjectVector types;
    types.reserve(expressions.size());
    for (const auto& expr : expressions) {
        types.push_back(visit(expr));
    }
    return types;
}

// Primitives
Object_ptr SemanticAnalyzer::visit(int expr) { return type_system->type_pool->get_int_type(); }
Object_ptr SemanticAnalyzer::visit(double expr) { return type_system->type_pool->get_float_type(); }
Object_ptr SemanticAnalyzer::visit(std::string expr) { return type_system->type_pool->get_string_type(); }
Object_ptr SemanticAnalyzer::visit(bool expr) { return type_system->type_pool->get_boolean_type(); }

// Identifiers
Object_ptr SemanticAnalyzer::visit(Identifier& expr)
{
    if (!current_scope->lookup_success(expr.name)) {
        FATAL("Undefined variable: " + expr.name);
    }

    return current_scope->lookup(expr.name)->type;
}

// Operators
Object_ptr SemanticAnalyzer::visit(Prefix& expr)
{
    Object_ptr right_type = visit(expr.right);
    return type_system->infer(current_scope, right_type, expr.op);
}

Object_ptr SemanticAnalyzer::visit(Infix& expr)
{
    Object_ptr left_type = visit(expr.left);
    Object_ptr right_type = visit(expr.right);
    return type_system->infer(current_scope, left_type, expr.op, right_type);
}

Object_ptr SemanticAnalyzer::visit(Postfix& expr)
{
    Object_ptr left_type = visit(expr.left);
    return type_system->infer(current_scope, left_type, expr.op);
}

// Collections

Object_ptr SemanticAnalyzer::visit(ListLiteral& expr)
{
	ObjectVector types = visit(expr.expressions);
	types = remove_duplicates(types);

	if (types.size() == 1)
	{
		Object_ptr list_type = MAKE_OBJECT_VARIANT(ListType(types.front()));
		return list_type;
	}

	Object_ptr variant_type = MAKE_OBJECT_VARIANT(VariantType(types));
	Object_ptr list_type = MAKE_OBJECT_VARIANT(ListType(variant_type));
	return list_type;
}

Object_ptr SemanticAnalyzer::visit(TupleLiteral& expr)
{
    ObjectVector element_types = visit(expr.elements);
    return MAKE_OBJECT_VARIANT(TupleType(element_types));
}

// Assignments
Object_ptr SemanticAnalyzer::visit(UntypedAssignment& expr)
{
    Object_ptr right_type = visit(expr.right);
    // You will need to extract the identifier name from expr.left here 
    // and check if it's mutable and assignable in current_scope.
    return right_type; 
}

Object_ptr SemanticAnalyzer::visit(TypedAssignment& expr)
{
    Object_ptr explicit_type = visit(expr.type_annotation);
    Object_ptr right_type = visit(expr.right);

    if (!type_system->assignable(current_scope, explicit_type, right_type)) {
        FATAL("Assigned value does not match explicit type.");
    }
    return explicit_type;
}

// Stubs for complex expressions
Object_ptr SemanticAnalyzer::visit(MapLiteral& expr) { FATAL("Not implemented"); return nullptr; }
Object_ptr SemanticAnalyzer::visit(SetLiteral& expr) { FATAL("Not implemented"); return nullptr; }
Object_ptr SemanticAnalyzer::visit(RangeLiteral& expr) { FATAL("Not implemented"); return nullptr; }
Object_ptr SemanticAnalyzer::visit(TypePattern& expr) { FATAL("Not implemented"); return nullptr; }
Object_ptr SemanticAnalyzer::visit(VariableDefinitionExpression& expr) { FATAL("Not implemented"); return nullptr; }
Object_ptr SemanticAnalyzer::visit(IfTernaryBranch& expr) { FATAL("Not implemented"); return nullptr; }
Object_ptr SemanticAnalyzer::visit(ElseTernaryBranch& expr) { FATAL("Not implemented"); return nullptr; }
Object_ptr SemanticAnalyzer::visit(Call& expr) { FATAL("Not implemented"); return nullptr; }
Object_ptr SemanticAnalyzer::visit(DotLiteral& expr) { FATAL("Not implemented"); return nullptr; }
Object_ptr SemanticAnalyzer::visit(DotDotLiteral& expr) { FATAL("Not implemented"); return nullptr; }
Object_ptr SemanticAnalyzer::visit(DotDotDotLiteral& expr) { FATAL("Not implemented"); return nullptr; }

Object_ptr SemanticAnalyzer::infer_chain_member_type(Object_ptr lhs, Expression_ptr expr, bool null_check) { FATAL("Not implemented"); return nullptr; }
}
