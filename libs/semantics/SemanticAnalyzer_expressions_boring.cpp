#include "AST.h"
#include "Doctor.h"
#include "Expression.h"
#include "Objects.h"
#include "SemanticAnalyzer.h"
#include "Statement.h"
#include "SymbolScope.h"
#include "Token.h"

#include <ctime>
#include <map>
#include <memory>
#include <string>
#include <variant>

#define make_object(x) std::make_shared<Object>(x)

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{
void SemanticAnalyzer::visit(ExpressionStatement& statement) { visit(statement.expression); }

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

Object_ptr SemanticAnalyzer::visit(const Expression_ptr expr)
{
    Doctor::get().fatal_if_nullptr(expr, WaspStage::Semantics);

    return std::visit(
        overloaded{
            // Primitives
            [&](int& node) -> Object_ptr { return visit(node); },
            [&](double& node) -> Object_ptr { return visit(node); },
            [&](std::string& node) -> Object_ptr { return visit(node); },
            [&](bool& node) -> Object_ptr { return visit(node); },

            [&](DotLiteral& node) -> Object_ptr { return visit(node); },

            // Identifiers & Access
            [&](Identifier& node) -> Object_ptr { return visit(node); },
            [&](MemberAccess& node) -> Object_ptr { return visit(node); },

            [&](Call& node) -> Object_ptr { return visit(node); },

            // Operators
            [&](Prefix& node) -> Object_ptr { return visit(node); },
            [&](Infix& node) -> Object_ptr { return visit(node); },
            [&](Postfix& node) -> Object_ptr { return visit(node); },

            // Collections
            [&](ListLiteral& node) -> Object_ptr { return visit(node); },
            [&](TupleLiteral& node) -> Object_ptr { return visit(node); },
            [&](MapLiteral& node) -> Object_ptr { return visit(node); },
            [&](SetLiteral& node) -> Object_ptr { return visit(node); },
            [&](RangeLiteral& node) -> Object_ptr { return visit(node); },

            // Variables & Assignments
            [&](VariableDefinitionExpression& node) -> Object_ptr { return visit(node); },
            [&](UntypedAssignment& node) -> Object_ptr { return visit(node); },
            [&](TypedAssignment& node) -> Object_ptr { return visit(node); },
            [&](TypePattern& node) -> Object_ptr { return visit(node); },

            // Control Flow
            [&](IfTernaryBranch& node) -> Object_ptr { return visit(node); },
            [&](ElseTernaryBranch& node) -> Object_ptr { return visit(node); },

            // Fallback
            [](auto&) -> Object_ptr
            {
                Doctor::get().Doctor::get().fatal(
                    WaspStage::Semantics,
                    "Unhandled Expression in Semantic Analyzer!");
            }},
        expr->data);
}

// -----------------------------------------------
// Simple Boring Expressions
// -----------------------------------------------

Object_ptr SemanticAnalyzer::visit(int expr)
{
    return make_object(IntType());
}

Object_ptr SemanticAnalyzer::visit(double expr)
{
    return make_object(FloatType());
}

Object_ptr SemanticAnalyzer::visit(std::string expr)
{
    return make_object(StringType());
}

Object_ptr SemanticAnalyzer::visit(bool expr)
{
    return make_object(BooleanType());
}

Object_ptr SemanticAnalyzer::visit(DotLiteral& expr) { return nullptr; }

Object_ptr SemanticAnalyzer::visit(Prefix& expr)
{
    Object_ptr right_type = visit(expr.operand);
    return type_checker->infer(current_scope, right_type, expr.op.type);
}

Object_ptr SemanticAnalyzer::visit(Infix& expr)
{
    Object_ptr left_type = visit(expr.left);
    Object_ptr right_type = visit(expr.right);
    return type_checker->infer(current_scope, left_type, expr.op.type, right_type);
}

Object_ptr SemanticAnalyzer::visit(Postfix& expr)
{
    Object_ptr left_type = visit(expr.operand);
    type_checker->expect_number_type(left_type);
    return left_type;
}

Object_ptr SemanticAnalyzer::visit(ListLiteral& expr)
{
    if (expr.expressions.empty())
        return make_object(ListType(make_object(AnyType())));

    ObjectVector element_types;
    for (const auto& element : expr.expressions)
        element_types.push_back(visit(element));

    ObjectVector unique_types = type_checker->remove_duplicates(current_scope, element_types);

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
        return make_object(MapType(make_object(AnyType()), make_object(AnyType())));

    ObjectVector key_types;
    ObjectVector val_types;

    for (const auto& [key_expr, val_expr] : expr.pairs)
    {
        Object_ptr k_type = visit(key_expr);
        type_checker->expect_key_type(current_scope, k_type);
        key_types.push_back(k_type);
        val_types.push_back(visit(val_expr));
    }

    ObjectVector unique_keys = type_checker->remove_duplicates(current_scope, key_types);
    ObjectVector unique_vals = type_checker->remove_duplicates(current_scope, val_types);

    Object_ptr final_key_type = unique_keys.size() == 1 ? unique_keys[0]
                                                        : make_object(VariantType(unique_keys));
    Object_ptr final_val_type = unique_vals.size() == 1 ? unique_vals[0]
                                                        : make_object(VariantType(unique_vals));

    return make_object(MapType(final_key_type, final_val_type));
}

Object_ptr SemanticAnalyzer::visit(SetLiteral& expr)
{
    if (expr.expressions.empty())
        return make_object(SetType(make_object(AnyType())));

    ObjectVector element_types;
    for (const auto& element : expr.expressions)
    {
        Object_ptr el_type = visit(element);
        type_checker->expect_key_type(current_scope, el_type);
        element_types.push_back(el_type);
    }

    ObjectVector unique_types = type_checker->remove_duplicates(current_scope, element_types);

    if (unique_types.size() == 1)
        return make_object(SetType(unique_types[0]));
    return make_object(SetType(make_object(VariantType(unique_types))));
}

Object_ptr SemanticAnalyzer::visit(RangeLiteral& expr)
{
    Object_ptr start_type = nullptr;
    Object_ptr end_type = nullptr;

    if (expr.start)
    {
        start_type = visit(expr.start);
        type_checker->expect_number_type(start_type);
    }

    if (expr.end)
    {
        end_type = visit(expr.end);
        type_checker->expect_number_type(end_type);
    }

    if (type_checker->is_float_type(start_type) || type_checker->is_float_type(end_type))
    {
        return make_object(ListType(make_object(FloatType())));
    }

    return make_object(ListType(make_object(IntType())));
}

Object_ptr SemanticAnalyzer::visit(TypePattern& expr)
{
    Doctor::get().fatal(
        WaspStage::Semantics,
        "Type patterns are not supported in expressions. They can only be used in match patterns."
    );
}

} // namespace Wasp
