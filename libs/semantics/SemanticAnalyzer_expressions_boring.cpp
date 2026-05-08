#include "AST.h"
#include "Doctor.h"
#include "Expression.h"
#include "Objects.h"
#include "SemanticAnalyzer.h"
#include "Statement.h"
#include "SymbolScope.h"
#include "Workspace.h"

#include <cctype>
#include <ctime>
#include <memory>
#include <string>
#include <variant>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{


// ============================================================================
// Main Visitor Dispatcher
// ============================================================================

Object_ptr SemanticAnalyzer::visit(const Expression_ptr expr)
{
    Doctor::get().fatal_if_nullptr(expr, WaspStage::Semantics);

    return std::visit(
        [&](auto& node) -> Object_ptr
        {
            if constexpr (requires { this->visit(node); })
            {
                return this->visit(node);
            }
            else
            {
                Doctor::get().fatal(
                    WaspStage::Semantics,
                    "Unhandled Expression in Semantic Analyzer"
                );
            }
        },
        expr->data
    );
}

void SemanticAnalyzer::visit(ExpressionStatement& statement)
{
    visit(statement.expression);
}

ObjectVector SemanticAnalyzer::visit(ExpressionVector expressions)
{
    ObjectVector computed_types;
    computed_types.reserve(expressions.size());
    for (const auto& expr : expressions)
    {
        computed_types.push_back(visit(expr));
    }
    return computed_types;
}

// ============================================================================
// Primitives & Operators
// ============================================================================

Object_ptr SemanticAnalyzer::visit(int expr)
{
    return make_object(IntLiteralType(expr));
}

Object_ptr SemanticAnalyzer::visit(double expr)
{
    return make_object(FloatLiteralType(expr));
}

Object_ptr SemanticAnalyzer::visit(std::string expr)
{
    return make_object(StringLiteralType(expr));
}

Object_ptr SemanticAnalyzer::visit(bool expr)
{
    return expr ? workspace->pool->get_true_literal_type()
                : workspace->pool->get_false_literal_type();
}

Object_ptr SemanticAnalyzer::visit(NoneLiteral& expr)
{
    return workspace->pool->get_none_type();
}

Object_ptr SemanticAnalyzer::visit(DotLiteral& expr)
{
    Doctor::get().fatal(WaspStage::Semantics, "Dot Literals are not supported yet");
}

Object_ptr SemanticAnalyzer::visit(Prefix& expr)
{
    return type_system->infer(current_scope, visit(expr.operand), expr.op.type);
}

Object_ptr SemanticAnalyzer::visit(Infix& expr)
{
    return type_system
        ->infer(current_scope, visit(expr.left), expr.op.type, visit(expr.right));
}

Object_ptr SemanticAnalyzer::visit(Postfix& expr)
{
    Object_ptr left_type = visit(expr.operand);
    type_system->expect_number_type(left_type);
    return left_type;
}

// ============================================================================
// Collections
// ============================================================================

Object_ptr SemanticAnalyzer::visit(ListLiteral& expr)
{
    if (expr.expressions.empty())
        return make_object(ListType(make_object(NativeAnyType())));

    ObjectVector element_types;
    for (const auto& element : expr.expressions)
        element_types.push_back(visit(element));

    ObjectVector unique_types = type_system->remove_duplicates(
        current_scope,
        element_types
    );
    if (unique_types.size() == 1)
        return make_object(ListType(unique_types[0]));
    return make_object(ListType(make_object(VariantType(unique_types))));
}

Object_ptr SemanticAnalyzer::visit(TupleLiteral& expr)
{
    ObjectVector element_types;
    for (const auto& element : expr.expressions)
        element_types.push_back(visit(element));
    return make_object(TupleType(element_types));
}

Object_ptr SemanticAnalyzer::visit(MapLiteral& expr)
{
    if (expr.pairs.empty())
        return make_object(
            MapType(make_object(NativeAnyType()), make_object(NativeAnyType()))
        );

    ObjectVector key_types, val_types;
    for (const auto& [k_expr, v_expr] : expr.pairs)
    {
        Object_ptr k_type = visit(k_expr);
        type_system->expect_key_type(current_scope, k_type);
        key_types.push_back(k_type);
        val_types.push_back(visit(v_expr));
    }

    ObjectVector u_keys = type_system->remove_duplicates(current_scope, key_types);
    ObjectVector u_vals = type_system->remove_duplicates(current_scope, val_types);

    Object_ptr fk = u_keys.size() == 1 ? u_keys[0] : make_object(VariantType(u_keys));
    Object_ptr fv = u_vals.size() == 1 ? u_vals[0] : make_object(VariantType(u_vals));
    return make_object(MapType(fk, fv));
}

Object_ptr SemanticAnalyzer::visit(SetLiteral& expr)
{
    if (expr.expressions.empty())
        return make_object(SetType(make_object(NativeAnyType())));

    ObjectVector element_types;
    for (const auto& element : expr.expressions)
    {
        Object_ptr el_type = visit(element);
        type_system->expect_key_type(current_scope, el_type);
        element_types.push_back(el_type);
    }

    ObjectVector unique_types = type_system->remove_duplicates(
        current_scope,
        element_types
    );
    if (unique_types.size() == 1)
        return make_object(SetType(unique_types[0]));
    return make_object(SetType(make_object(VariantType(unique_types))));
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
        return make_object(ListType(make_object(NativeFloatType())));
    }

    return make_object(ListType(make_object(NativeIntType())));
}

Object_ptr SemanticAnalyzer::visit(TypePattern& expr)
{
    Doctor::get().fatal(WaspStage::Semantics, "Type patterns can only be used in match patterns.");
}

} // namespace Wasp
