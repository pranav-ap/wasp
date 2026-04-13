#include "AST.h"
#include "Doctor.h"
#include "Expression.h"
#include "Objects.h"
#include "SemanticAnalyzer.h"
#include "SymbolScope.h"
#include "Workspace.h"

#include <ctime>
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
        const auto module_type = left_type->as<std::shared_ptr<ModuleType>>();

        expr.member_index = module_type->get_member_index(member_name);
        return module_type->get_member_type(member_name);
    }
    else if (left_type->is<std::shared_ptr<ClassType>>())
    {
        const auto class_type = left_type->as<std::shared_ptr<ClassType>>();

        expr.member_index = class_type->get_member_index(member_name);
        return class_type->get_member_type(member_name);
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

            [&](auto&) -> Object_ptr
            {
                Doctor::get().fatal(
                    WaspStage::Semantics,
                    "Expected an Identifier or MemberAccess as the callable."
                );

                return workspace->pool->get_none_type();
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
    auto [group_symbol, function_symbol, overload_index] = type_checker->resolve_function_call(
        current_scope,
        callable_identifier.name,
        arg_types
    );

    callable_identifier.symbol = group_symbol;

    if (function_symbol->should_be_captured(current_scope->get_closure_depth()))
    {
        callable_identifier.must_be_captured = true;
    }

    if (is_native_function(function_symbol))
    {
        call_expr.overload_index = -1;
    }
    else
    {
        call_expr.overload_index = overload_index;
    }

    return get_function_return_type(function_symbol);
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

    // Safely get as ContainerType to support both ClassType and TraitType
    std::shared_ptr<ContainerType> container_type;
    if (auto p = left_type->try_as<std::shared_ptr<ClassType>>())
        container_type = *p;
    else if (auto p = left_type->try_as<std::shared_ptr<TraitType>>())
        container_type = *p;

    auto [group_symbol, function_symbol, overload_index] = type_checker->resolve_class_method_call(
        current_scope,
        container_type->name,
        right_id.name,
        arg_types
    );

    mac.member_index = container_type->get_member_index(right_id.name);

    call_expr.overload_index = overload_index;
    right_id.symbol = function_symbol;

    call_expr.is_method_call = true;

    return get_function_return_type(function_symbol);
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

    auto class_type_obj = symbol->get_type();
    auto class_type = class_type_obj->as<std::shared_ptr<ClassType>>();

    StringVector instance_members = class_type->get_instance_variables_declaration_order();

    Doctor::get().assert(
        arg_types.size() == instance_members.size(),
        WaspStage::Semantics,
        "Constructor Arguments Count Mismatch. Expected " +
            std::to_string(instance_members.size()) + ", got " + std::to_string(arg_types.size())
    );

    for (size_t i = 0; i < instance_members.size(); ++i)
    {
        const std::string& member_name = instance_members[i];

        Object_ptr expected_type = class_type->get_member_type(member_name);
        Object_ptr actual_type = arg_types[i];

        Doctor::get().assert(
            type_checker->assignable(current_scope, expected_type, actual_type),
            WaspStage::Semantics,
            "Constructor Arguments Type Mismatch for member '" + member_name + "'"
        );
    }

    return class_type_obj;
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

    auto [function_symbol, overload_index, member_index] = type_checker
                                                               ->resolve_module_function_call(
                                                                   current_scope,
                                                                   left_id.name,
                                                                   right_id.name,
                                                                   arg_types
                                                               );

    mac.member_index = member_index;
    call_expr.overload_index = overload_index;
    right_id.symbol = function_symbol;

    return get_function_return_type(function_symbol);
}

} // namespace Wasp
