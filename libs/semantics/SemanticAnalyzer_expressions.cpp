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

Object_ptr SemanticAnalyzer::visit(MemberAccess& access)
{
    Object_ptr receiver_type = visit(access.left);

    Doctor::get().assert(
        access.right->is<Identifier>(),
        WaspStage::Semantics,
        "RHS of member access must be an identifier."
    );

    std::string target_name = access.right->as<Identifier>().name;

    if (receiver_type->is<std::shared_ptr<ModuleType>>())
    {
        const auto module_ref = receiver_type->as<std::shared_ptr<ModuleType>>();

        access.member_index = module_ref->get_member_index(target_name);
        return module_ref->get_member_type(target_name);
    }
    else if (receiver_type->is<std::shared_ptr<ClassType>>())
    {
        const auto class_ref = receiver_type->as<std::shared_ptr<ClassType>>();

        access.member_index = class_ref->get_member_index(target_name);
        return class_ref->get_member_type(target_name);
    }

    Doctor::get().fatal(
        WaspStage::Semantics,
        "Cannot access member '" + target_name +
            "'. The left-hand side is neither a module nor a class instance."
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

                if (receiver_type->is<std::shared_ptr<ClassType>>())
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

                return workspace->pool->get_none_type();
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
                                                                     receiver_class->name,
                                                                     method_identifier.name,
                                                                     argument_types
                                                                 );

    access.member_index = receiver_class->get_member_index(method_identifier.name);

    call.overload_index = overload_index;
    method_identifier.symbol = resolved_method;

    call.is_method_call = true;

    return get_function_return_type(resolved_method);
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

    StringVector instance_fields = class_type->get_instance_variable_names_in_declaration_order();

    Doctor::get().assert(
        argument_types.size() == instance_fields.size(),
        WaspStage::Semantics,
        "Constructor Arguments Count Mismatch. Expected " + std::to_string(instance_fields.size()) +
            ", got " + std::to_string(argument_types.size())
    );

    for (size_t i = 0; i < instance_fields.size(); ++i)
    {
        const std::string& field_name = instance_fields[i];

        Object_ptr expected_type = class_type->get_member_type(field_name);
        Object_ptr actual_type = argument_types[i];

        Doctor::get().assert(
            type_checker->assignable(current_scope, expected_type, actual_type),
            WaspStage::Semantics,
            "Constructor Arguments Type Mismatch for member '" + field_name + "'"
        );
    }

    return class_type_obj;
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
