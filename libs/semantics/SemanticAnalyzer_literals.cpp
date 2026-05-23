#include "AST.h"
#include "Doctor.h"
#include "Expression.h"
#include "Objects.h"
#include "SemanticAnalyzer.h"
#include "Workspace.h"

#include <cctype>
#include <ctime>
#include <string>

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

Object_ptr SemanticAnalyzer::visit(InterpolatedString& node)
{
    for (auto& part : node.parts)
    {
        visit(part);
    }

    return workspace->pool->get_string_type();
}

Object_ptr SemanticAnalyzer::visit(DotLiteral& expr)
{
    Doctor::get().fatal(WaspStage::Semantics, "Dot Literals are not supported yet");
}

Object_ptr SemanticAnalyzer::collapse_types(const ObjectVector& types)
{
    if (types.empty())
    {
        return make_object(AnyType());
    }

    ObjectVector unique_types = type_system->remove_duplicates(current_scope, types);

    if (unique_types.size() == 1)
    {
        return unique_types[0];
    }

    return make_object(VariantType(unique_types));
}

Object_ptr SemanticAnalyzer::visit(ListLiteral& expr)
{
    return make_object(ListType(collapse_types(visit(expr.expressions))));
}

Object_ptr SemanticAnalyzer::visit(TupleLiteral& expr)
{
    return make_object(TupleType(visit(expr.expressions)));
}

Object_ptr SemanticAnalyzer::visit(SetLiteral& expr)
{
    ObjectVector types = visit(expr.expressions);
    for (auto& type : types)
    {
        type_system->expect_key_type(current_scope, type);
    }
    return make_object(SetType(collapse_types(types)));
}

Object_ptr SemanticAnalyzer::visit(MapLiteral& expr)
{
    ObjectVector key_types, val_types;
    for (const auto& [k_expr, v_expr] : expr.pairs)
    {
        Object_ptr k_type = visit(k_expr);
        type_system->expect_key_type(current_scope, k_type);
        key_types.push_back(k_type);
        val_types.push_back(visit(v_expr));
    }

    return make_object(
        MapType(collapse_types(key_types), collapse_types(val_types))
    );
}

Object_ptr SemanticAnalyzer::visit(RangeLiteral& expr)
{
    Object_ptr start_type = expr.start ? visit(expr.start) : nullptr;
    Object_ptr end_type = expr.end ? visit(expr.end) : nullptr;

    if (start_type)
    {
        type_system->expect_number_type(start_type);
    }

    if (end_type)
    {
        type_system->expect_number_type(end_type);
    }

    if (type_system->is_float_type(start_type) ||
        type_system->is_float_type(end_type))
    {
        return make_object(ListType(make_object(FloatType())));
    }

    return make_object(ListType(make_object(IntType())));
}

} // namespace Wasp
