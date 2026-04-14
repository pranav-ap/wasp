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

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

Object_ptr SemanticAnalyzer::visit(Identifier& identifier)
{
    Symbol_ptr resolved_symbol = current_scope->lookup(identifier.name);
    Doctor::get().fatal_if_nullptr(resolved_symbol, WaspStage::Semantics);

    identifier.symbol = resolved_symbol;

    if (resolved_symbol->should_be_captured(current_scope->get_closure_depth()))
    {
        identifier.must_be_captured = true;
    }

    return resolved_symbol->get_type();
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
        const auto module_ref = left_type->as<std::shared_ptr<ModuleType>>();

        expr.member_index = module_ref->get_member_index(member_name);
        return module_ref->get_member_type(member_name);
    }
    else if (left_type->is<std::shared_ptr<ClassType>>())
    {
        const auto class_ref = left_type->as<std::shared_ptr<ClassType>>();

        expr.member_index = class_ref->get_member_index(member_name);
        return class_ref->get_member(member_name);
    }

    Doctor::get().fatal(
        WaspStage::Semantics,
        "Cannot access member " + member_name +
            ". The left-hand side is neither a module nor a class instance."
    );
}

Object_ptr SemanticAnalyzer::visit(Call& call)
{
    ObjectVector argument_types;

    for (auto& arg : call.arguments)
    {
        argument_types.push_back(visit(arg));
    }

    return std::visit(
        overloaded{
            [&](Identifier& target) -> Object_ptr
            {
                auto resolved_symbol = current_scope->lookup(target.name);
                Doctor::get().fatal_if_nullptr(resolved_symbol, WaspStage::Semantics);

                if (resolved_symbol->payload_is<ClassData>())
                {
                    return evaluate_instance_creation(
                        call,
                        target,
                        resolved_symbol,
                        argument_types
                    );
                }

                return evaluate_identifier_call(call, target, argument_types);
            },

            [&](MemberAccess& access) -> Object_ptr
            {
                Object_ptr receiver_type = visit(access.left);

                if (receiver_type->is<ClassType_ptr>())
                {
                    return evaluate_instance_method_call(
                        call,
                        access,
                        argument_types,
                        receiver_type
                    );
                }

                return evaluate_module_method_call(call, access, argument_types);
            },

            [&](auto&) -> Object_ptr
            {
                Doctor::get().fatal(
                    WaspStage::Semantics,
                    "Expected an Identifier or MemberAccess as the callable."
                );
            }
        },
        call.callable->data
    );
}

Object_ptr SemanticAnalyzer::evaluate_identifier_call(
    Call& call,
    Identifier& target,
    const ObjectVector& argument_types
)
{
    auto [overload_group, resolved_function, overload_index] = type_checker->resolve_function_call(
        current_scope,
        target.name,
        argument_types
    );

    target.symbol = overload_group;

    if (resolved_function->should_be_captured(current_scope->get_closure_depth()))
    {
        target.must_be_captured = true;
    }

    if (is_native_function(resolved_function))
    {
        call.overload_index = -1;
    }
    else
    {
        call.overload_index = overload_index;
    }

    return get_function_return_type(resolved_function);
}

Object_ptr SemanticAnalyzer::evaluate_instance_creation(
    Call& call,
    Identifier& target,
    Symbol_ptr class_symbol,
    ObjectVector argument_types
)
{
    call.is_constructor_call = true;
    target.symbol = class_symbol;

    if (class_symbol->should_be_captured(current_scope->get_closure_depth()))
    {
        target.must_be_captured = true;
    }

    auto class_type_obj = class_symbol->get_type();
    auto class_type = class_type_obj->as<std::shared_ptr<ClassType>>();

    Doctor::get().assert(
        argument_types.size() == class_type->fields.size(),
        WaspStage::Semantics,
        "Constructor Arguments Count Mismatch. Expected " +
            std::to_string(class_type->fields.size()) + ", got " +
            std::to_string(argument_types.size())
    );

    for (size_t i = 0; i < class_type->fields.size(); ++i)
    {
        Object_ptr expected_type = class_type->get_member(class_type->fields[i]);
        Object_ptr actual_type = argument_types[i];

        Doctor::get().assert(
            type_checker->assignable(current_scope, expected_type, actual_type),
            WaspStage::Semantics,
            "Constructor Arguments Type Mismatch for member '" + class_type->fields[i] + "'"
        );
    }

    return class_type_obj;
}

Object_ptr SemanticAnalyzer::evaluate_instance_method_call(
    Call& call,
    MemberAccess& access,
    const ObjectVector& argument_types,
    Object_ptr receiver_type
)
{
    Doctor::get().assert(
        access.right->is<Identifier>(),
        WaspStage::Semantics,
        "Method name must be an identifier."
    );

    auto& method_identifier = access.right->as<Identifier>();

    Doctor::get().assert(
        receiver_type->is<std::shared_ptr<ClassType>>(),
        WaspStage::Semantics,
        "Target of method call must be a class instance."
    );

    auto receiver_class = receiver_type->as<std::shared_ptr<ClassType>>();

    auto [overload_group, resolved_method, overload_index] = type_checker
                                                                 ->resolve_class_method_call(
                                                                     current_scope,
                                                                     receiver_class,
                                                                     method_identifier.name,
                                                                     argument_types
                                                                 );

    access.member_index = receiver_class->get_member_index(method_identifier.name);

    call.overload_index = overload_index;
    method_identifier.symbol = resolved_method;

    call.is_method_call = true;

    return get_function_return_type(resolved_method);
}

Object_ptr SemanticAnalyzer::evaluate_module_method_call(
    Call& call,
    MemberAccess& access,
    const ObjectVector& argument_types
)
{
    Doctor::get().assert(
        access.left->is<Identifier>() && access.right->is<Identifier>(),
        WaspStage::Semantics,
        "Supports calls of the form module.function()"
    );

    auto& module_identifier = access.left->as<Identifier>();
    auto& method_identifier = access.right->as<Identifier>();

    Symbol_ptr module_symbol = current_scope->lookup(module_identifier.name);
    Doctor::get().fatal_if_nullptr(module_symbol, WaspStage::Semantics);

    module_identifier.symbol = module_symbol;

    if (module_symbol->should_be_captured(current_scope->get_closure_depth()))
    {
        module_identifier.must_be_captured = true;
    }

    auto& module_data = module_symbol->get_payload_as<ModuleData>();
    Symbol_ptr member_symbol = module_data.mod->get_member(method_identifier.name);

    Doctor::get().fatal_if_nullptr(
        member_symbol,
        WaspStage::Semantics,
        "Member '" + method_identifier.name + "' not found in module."
    );

    if (member_symbol->payload_is<ClassData>())
    {
        access.member_index = module_data.mod->get_member_index(method_identifier.name);

        return evaluate_instance_creation(call, method_identifier, member_symbol, argument_types);
    }

    auto
        [overload_group,
         resolved_function,
         overload_index,
         module_member_index] = type_checker
                                    ->resolve_module_function_call(
                                        current_scope,
                                        module_identifier.name,
                                        method_identifier.name,
                                        argument_types
                                    );

    access.member_index = module_member_index;
    call.overload_index = overload_index;
    method_identifier.symbol = resolved_function;

    return get_function_return_type(resolved_function);
}

} // namespace Wasp
