#include "AST.h"
#include "Doctor.h"
#include "Expression.h"
#include "Objects.h"
#include "SemanticAnalyzer.h"
#include "Workspace.h"

#include <cctype>
#include <ctime>
#include <memory>

namespace Wasp
{

Object_ptr SemanticAnalyzer::visit(IntegerLiteral& expr)
{
    return workspace->pool->get_int_type();
}

Object_ptr SemanticAnalyzer::visit(FloatLiteral& expr)
{
    return workspace->pool->get_float_type();
}

Object_ptr SemanticAnalyzer::visit(StringLiteral& expr)
{
    return workspace->pool->get_string_type();
}

Object_ptr SemanticAnalyzer::visit(BooleanLiteral& expr)
{
    return workspace->pool->get_boolean_type();
}

Object_ptr SemanticAnalyzer::visit(NoneLiteral& expr)
{
    return workspace->pool->get_none_type();
}

Object_ptr SemanticAnalyzer::visit(ListLiteral& expr)
{
    ObjectVector element_types = visit(expr.expressions);
    Object_ptr unified_element_type = type_system->unify(
        current_scope,
        element_types
    );

    return make_object(std::make_shared<ListType>(unified_element_type));
}

Object_ptr SemanticAnalyzer::visit(TupleLiteral& expr)
{
    ObjectVector element_types = visit(expr.expressions);

    return make_object(std::make_shared<TupleType>(element_types));
}

Object_ptr SemanticAnalyzer::visit(SetLiteral& expr)
{
    ObjectVector types = visit(expr.expressions);

    for (auto& type : types)
    {
        Doctor::get().assert(
            type_system->is_key_type(current_scope, type),
            WaspStage::Semantics,
            "Invalid set element type: " + type->to_string()
        );
    }

    Object_ptr unified_type = type_system->unify(current_scope, types);

    return make_object(std::make_shared<SetType>(unified_type));
}

Object_ptr SemanticAnalyzer::visit(MapLiteral& expr)
{
    ObjectVector key_types, val_types;

    for (const auto& [k_expr, v_expr] : expr.pairs)
    {
        Object_ptr k_type = visit(k_expr);

        Doctor::get().assert(
            type_system->is_key_type(current_scope, k_type),
            WaspStage::Semantics,
            "Invalid map key type: " + k_type->to_string()
        );

        key_types.push_back(k_type);
        val_types.push_back(visit(v_expr));
    }

    Object_ptr unified_key_type = type_system->unify(current_scope, key_types);
    Object_ptr unified_val_type = type_system->unify(current_scope, val_types);

    return make_object(
        std::make_shared<MapType>(unified_key_type, unified_val_type)
    );
}

} // namespace Wasp
