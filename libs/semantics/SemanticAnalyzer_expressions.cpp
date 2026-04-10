#include "AST.h"
#include "Doctor.h"
#include "Expression.h"
#include "Objects.h"
#include "SemanticAnalyzer.h"
#include "Statement.h"
#include "SymbolScope.h"
#include "Token.h"
#include "Workspace.h"

#include <algorithm>
#include <ctime>
#include <map>
#include <memory>
#include <string>
#include <variant>
#include <vector>

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

// -----------------------------------------------
// Simple Boring Expressions
// -----------------------------------------------

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

// -----------------------------------------------
// Identifiers, Member Access & Calls
// -----------------------------------------------

Object_ptr SemanticAnalyzer::visit(Identifier& expr)
{
    Symbol_ptr symbol = current_scope->lookup(expr.name);
    Doctor::get().fatal_if_nullptr(symbol, WaspStage::Semantics);

    expr.symbol = symbol;

    if (symbol->should_be_captured(current_scope->get_closure_depth()))
    {
        expr.must_be_captured = true;
    }

    return symbol->get_type();
}

Object_ptr SemanticAnalyzer::visit(MemberAccess& expr)
{
    Object_ptr left_type = visit(expr.left);

    Doctor::get().assert(
        expr.right->is<Identifier>(),
        WaspStage::Semantics,
        "RHS of member access must be an identifier."
    );

    std::string member_name = expr.right->as<Identifier>().name;

    if (left_type->is<std::shared_ptr<ModuleType>>())
    {
        const auto& module_type = left_type->as<std::shared_ptr<ModuleType>>();

        expr.member_index = module_type->get_member_index(member_name);
        return module_type->get_member(member_name);
    }
    else if (left_type->is<std::shared_ptr<ClassType>>())
    {
        const auto class_type = left_type->as<std::shared_ptr<ClassType>>();

        expr.member_index = class_type->get_member_index(member_name);
        return class_type->get_member(member_name);
    }

    Doctor::get().fatal(
        WaspStage::Semantics,
        "Cannot access member '" + member_name +
            "'. The left-hand side is neither a module nor a class instance."
    );
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
            [&](Identifier& id) -> Object_ptr
            {
                auto symbol = current_scope->lookup(id.name);
                Doctor::get().fatal_if_nullptr(symbol, WaspStage::Semantics);

                if (symbol->payload_is<ClassData>())
                {
                    return evaluate_instance_creation(call_expr, id, symbol, arg_types);
                }

                return evaluate_identifier_call(call_expr, id, arg_types);
            },

            [&](MemberAccess& ma) -> Object_ptr
            {
                Object_ptr left_type = visit(ma.left);

                if (left_type->is<std::shared_ptr<ClassType>>())
                {
                    return evaluate_instance_method_call(call_expr, ma, arg_types, left_type);
                }

                return evaluate_module_method_call(call_expr, ma, arg_types);
            },

            [](auto&) -> Object_ptr
            {
                Doctor::get().fatal(
                    WaspStage::Semantics,
                    "Expected an Identifier or MemberAccess as the callable."
                );

                return MAKE_OBJECT_VARIANT(NoneType());
            }
        },
        call_expr.callable->data
    );
}
Object_ptr SemanticAnalyzer::evaluate_identifier_call(
    Call& call_expr,
    Identifier& callable_identifier,
    const ObjectVector& arg_types
)
{
    auto [function_symbol, overload_index] = type_checker
                                                 ->resolve_function_call(
                                                     current_scope,
                                                     callable_identifier.name,
                                                     arg_types
                                                 );

    Symbol_ptr group_symbol = current_scope->lookup(callable_identifier.name);
    callable_identifier.symbol = group_symbol;

    if (function_symbol->should_be_captured(current_scope->get_closure_depth()))
    {
        callable_identifier.must_be_captured = true;
    }

    if (function_symbol->get_payload_as<FunctionData>().is_native)
    {
        call_expr.overload_index = -1;
    }
    else
    {
        call_expr.overload_index = overload_index;
    }

    return function_symbol->get_payload_as<FunctionData>().get_return_type();
}

Object_ptr SemanticAnalyzer::evaluate_module_method_call(
    Call& call_expr,
    MemberAccess& mac,
    const ObjectVector& arg_types
)
{
    Doctor::get().assert(
        mac.left->is<Identifier>() && mac.right->is<Identifier>(),
        WaspStage::Semantics,
        "Supports calls of the form module.function()"
    );

    auto& left_id = mac.left->as<Identifier>();
    auto& right_id = mac.right->as<Identifier>();

    Symbol_ptr module_symbol = current_scope->lookup(left_id.name);
    Doctor::get().fatal_if_nullptr(module_symbol, WaspStage::Semantics);

    left_id.symbol = module_symbol;

    if (module_symbol->should_be_captured(current_scope->get_closure_depth()))
    {
        left_id.must_be_captured = true;
    }

    auto [function_symbol, overload_index, member_index] = type_checker->resolve_module_call(
        current_scope,
        left_id.name,
        right_id.name,
        arg_types
    );

    mac.member_index = member_index;
    call_expr.overload_index = overload_index;
    right_id.symbol = function_symbol;

    return function_symbol->get_payload_as<FunctionData>().get_return_type();
}

Object_ptr SemanticAnalyzer::evaluate_instance_method_call(
    Call& call_expr,
    MemberAccess& mac,
    const ObjectVector& arg_types,
    Object_ptr left_type
)
{
    Doctor::get().assert(
        mac.right->is<Identifier>(),
        WaspStage::Semantics,
        "Method name must be an identifier."
    );

    auto& right_id = mac.right->as<Identifier>();
    auto class_type = left_type->as<std::shared_ptr<ClassType>>();

    auto [function_symbol, overload_index] = type_checker->resolve_class_method_call(
        current_scope,
        class_type->class_name,
        right_id.name,
        arg_types
    );

    mac.member_index = class_type->get_member_index(right_id.name);

    call_expr.overload_index = overload_index;
    right_id.symbol = function_symbol;

    call_expr.is_method_call = true;

    return function_symbol->get_payload_as<FunctionData>().get_return_type();
}

Object_ptr SemanticAnalyzer::evaluate_instance_creation(
    Call& call_expr,
    Identifier& callable_identifier,
    Symbol_ptr symbol,
    ObjectVector arg_types
)
{
    call_expr.is_constructor_call = true;
    callable_identifier.symbol = symbol;

    if (symbol->should_be_captured(current_scope->get_closure_depth()))
    {
        callable_identifier.must_be_captured = true;
    }

    auto class_type_obj = symbol->get_payload_as<ClassData>().type;
    auto class_type = class_type_obj->as<std::shared_ptr<ClassType>>();

    std::vector<std::string> instance_members;

    for (const auto& member_name : class_type->values_declaration_order)
    {
        if (std::find(class_type->is_ours.begin(), class_type->is_ours.end(), member_name) ==
            class_type->is_ours.end())
        {
            instance_members.push_back(member_name);
        }
    }

    Doctor::get().assert(
        arg_types.size() == instance_members.size(),
        WaspStage::Semantics,
        "Constructor Arguments Count Mismatch. Expected " +
            std::to_string(instance_members.size()) + ", got " + std::to_string(arg_types.size())
    );

    for (size_t i = 0; i < instance_members.size(); ++i)
    {
        const std::string& member_name = instance_members[i];

        Object_ptr expected_type = class_type->get_member(member_name);
        Object_ptr actual_type = arg_types[i];

        Doctor::get().assert(
            type_checker->assignable(current_scope, expected_type, actual_type),
            WaspStage::Semantics,
            "Constructor Arguments Type Mismatch for member '" + member_name + "'"
        );
    }

    return class_type_obj;
}

} // namespace Wasp
