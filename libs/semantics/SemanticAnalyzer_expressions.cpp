#include "AST.h"
#include "Doctor.h"
#include "Expression.h"
#include "Objects.h"
#include "SemanticAnalyzer.h"
#include "Statement.h"
#include "Symbol.h"
#include "SymbolScope.h"
#include "Token.h"

#include <ctime>
#include <map>
#include <memory>
#include <string>
#include <variant>

#define MAKE_OBJECT_VARIANT(x) std::make_shared<Object>(x)

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

Object_ptr SemanticAnalyzer::visit(Identifier& expr)
{
    Symbol_ptr target_symbol = current_scope->lookup(expr.name);
    Doctor::get().fatal_if_nullptr(target_symbol, WaspStage::Semantics);

    expr.symbol = target_symbol;

    if (target_symbol->should_be_captured(current_scope->get_closure_depth()))
    {
        expr.must_be_captured = true;
    }

    return target_symbol->get_type();
}

Object_ptr SemanticAnalyzer::visit(MemberAccess& expr)
{
    // For `a.b.c`, this naturally evaluates `a.b` first, which evaluates `a`
    Object_ptr left_type = visit(expr.left);

    Doctor::get().assert(
        left_type->is<ModuleType>(),
        WaspStage::Semantics,
        "LHS of member access is not a module");

    const auto& module_type = left_type->as<ModuleType>();

    // The right side MUST be an identifier (e.g., 'c' in a.b.c)
    Doctor::get().assert(
        expr.right->is<Identifier>(),
        WaspStage::Semantics,
        "RHS of member access must be an identifier.");

    std::string member_name = expr.right->as<Identifier>().name;

    Doctor::get().assert(
        module_type.contains_member(member_name),
        WaspStage::Semantics,
        "Module does not contain member '" + member_name + "'");

    return module_type.get_member_type(member_name);
}

Object_ptr SemanticAnalyzer::visit(Call& call_expr)
{
    ObjectVector arg_types;

    for (auto& arg : call_expr.arguments)
    {
        arg_types.push_back(visit(arg));
    }

    return std::visit(
        overloaded{
            [&](Identifier& id) -> Object_ptr { return evaluate_identifier_call(id, arg_types); },

            [&](MemberAccess& ma) -> Object_ptr
            { return evaluate_member_access_call(call_expr.callable, arg_types); },

            [](auto&) -> Object_ptr
            {
                Doctor::get().fatal(
                    WaspStage::Semantics,
                    "Expected an Identifier or MemberAccess as the callable.");
            }},
        call_expr.callable->data);
}

Object_ptr SemanticAnalyzer::evaluate_identifier_call(
    Identifier& callable_identifier,
    const ObjectVector& arg_types)
{
    Symbol_ptr function_symbol = type_checker->resolve_function_overload(
        current_scope,
        callable_identifier.name,
        arg_types);

    callable_identifier.symbol = function_symbol;

    if (function_symbol->should_be_captured(current_scope->get_closure_depth()))
    {
        callable_identifier.must_be_captured = true;
    }

    return function_symbol->get_payload_as<FunctionData>().get_return_type();
}

Object_ptr SemanticAnalyzer::evaluate_member_access_call(
    const Expression_ptr& callable_expr,
    const ObjectVector& arg_types)
{
    auto& mac = callable_expr->as<MemberAccess>();

    Doctor::get().assert(
        mac.left->is<Identifier>() && mac.right->is<Identifier>(),
        WaspStage::Semantics,
        "Supports calls of the form module.function()");

    std::string module_name = mac.left->as<Identifier>().name;
    std::string func_name = mac.right->as<Identifier>().name;

    Symbol_ptr module_symbol = current_scope->lookup(module_name);
    Doctor::get().fatal_if_nullptr(module_symbol, WaspStage::Semantics);

    mac.left->as<Identifier>().symbol = module_symbol;

    if (module_symbol->should_be_captured(current_scope->get_closure_depth()))
    {
        mac.left->as<Identifier>().must_be_captured = true;
    }

    Symbol_ptr function_symbol = type_checker->resolve_module_function_overload(
        current_scope,
        module_name,
        func_name,
        arg_types);

    mac.right->as<Identifier>().symbol = function_symbol;

    return function_symbol->get_payload_as<FunctionData>().get_return_type();
}

Object_ptr SemanticAnalyzer::visit(int expr) { return MAKE_OBJECT_VARIANT(IntType()); }

Object_ptr SemanticAnalyzer::visit(double expr) { return MAKE_OBJECT_VARIANT(FloatType()); }

Object_ptr SemanticAnalyzer::visit(std::string expr) { return MAKE_OBJECT_VARIANT(StringType()); }

Object_ptr SemanticAnalyzer::visit(bool expr) { return MAKE_OBJECT_VARIANT(BooleanType()); }

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
        return MAKE_OBJECT_VARIANT(ListType(MAKE_OBJECT_VARIANT(AnyType())));

    ObjectVector element_types;
    for (const auto& element : expr.expressions)
        element_types.push_back(visit(element));

    ObjectVector unique_types = type_checker->remove_duplicates(current_scope, element_types);

    if (unique_types.size() == 1)
        return MAKE_OBJECT_VARIANT(ListType(unique_types[0]));
    return MAKE_OBJECT_VARIANT(ListType(MAKE_OBJECT_VARIANT(VariantType(unique_types))));
}

Object_ptr SemanticAnalyzer::visit(TupleLiteral& expr)
{
    ObjectVector element_types;
    for (const auto& element : expr.expressions)
        element_types.push_back(visit(element));
    return MAKE_OBJECT_VARIANT(TupleType(element_types));
}

Object_ptr SemanticAnalyzer::visit(MapLiteral& expr)
{
    if (expr.pairs.empty())
        return MAKE_OBJECT_VARIANT(
            MapType(MAKE_OBJECT_VARIANT(AnyType()), MAKE_OBJECT_VARIANT(AnyType())));

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

    Object_ptr final_key_type = unique_keys.size() == 1
                                    ? unique_keys[0]
                                    : MAKE_OBJECT_VARIANT(VariantType(unique_keys));
    Object_ptr final_val_type = unique_vals.size() == 1
                                    ? unique_vals[0]
                                    : MAKE_OBJECT_VARIANT(VariantType(unique_vals));

    return MAKE_OBJECT_VARIANT(MapType(final_key_type, final_val_type));
}

Object_ptr SemanticAnalyzer::visit(SetLiteral& expr)
{
    if (expr.expressions.empty())
        return MAKE_OBJECT_VARIANT(SetType(MAKE_OBJECT_VARIANT(AnyType())));

    ObjectVector element_types;
    for (const auto& element : expr.expressions)
    {
        Object_ptr el_type = visit(element);
        type_checker->expect_key_type(current_scope, el_type);
        element_types.push_back(el_type);
    }

    ObjectVector unique_types = type_checker->remove_duplicates(current_scope, element_types);

    if (unique_types.size() == 1)
        return MAKE_OBJECT_VARIANT(SetType(unique_types[0]));
    return MAKE_OBJECT_VARIANT(SetType(MAKE_OBJECT_VARIANT(VariantType(unique_types))));
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
        return MAKE_OBJECT_VARIANT(ListType(MAKE_OBJECT_VARIANT(FloatType())));
    }

    return MAKE_OBJECT_VARIANT(ListType(MAKE_OBJECT_VARIANT(IntType())));
}

Object_ptr SemanticAnalyzer::visit(TypePattern& expr) { return nullptr; }
} // namespace Wasp
