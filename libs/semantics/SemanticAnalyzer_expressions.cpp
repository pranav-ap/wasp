#include "AST.h"
#include "Doctor.h"
#include "Expression.h"
#include "Objects.h"
#include "SemanticAnalyzer.h"
#include "SymbolScope.h"
#include "Workspace.h"

#include <cctype>
#include <ctime>
#include <memory>
#include <string>
#include <variant>
#include <vector>

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

    if (left_type->is<ModuleType_ptr>())
    {
        const auto type = left_type->as<ModuleType_ptr>();

        expr.member_index = type->get_member_index(member_name);
        return type->get_member(member_name);
    }
    else if (left_type->is<ClassType_ptr>())
    {
        const auto type = left_type->as<ClassType_ptr>();

        expr.member_index = type->get_member_index(member_name);
        return type->get_member(member_name);
    }

    Doctor::get().fatal(
        WaspStage::Semantics,
        "Cannot access member " + member_name +
            ". The left-hand side is neither a module nor a class instance."
    );
}

Object_ptr SemanticAnalyzer::visit(Constructor& constructor)
{
    ObjectVector argument_types = visit(constructor.values);

    return std::visit(
        overloaded{
            [&](Identifier& target) -> Object_ptr
            {
                auto resolved_symbol = current_scope->lookup(target.name);
                Doctor::get().fatal_if_nullptr(resolved_symbol, WaspStage::Semantics);

                Doctor::get().assert(
                    resolved_symbol->payload_is<ClassData>(),
                    WaspStage::Semantics,
                    "Constructor target must be a class symbol."
                );

                return evaluate_instance_creation(
                    constructor,
                    target,
                    resolved_symbol,
                    argument_types
                );
            },
            [&](MemberAccess& access) -> Object_ptr
            {
                return evaluate_module_instance_creation(constructor, access, argument_types);
            },
            [&](auto&) -> Object_ptr
            {
                Doctor::get().fatal(
                    WaspStage::Semantics,
                    "Expected an Identifier or MemberAccess as the constructor target."
                );
                return nullptr;
            }
        },
        constructor.construtable->data
    );
}

Object_ptr SemanticAnalyzer::visit(Call& call)
{
    ObjectVector argument_types = visit(call.arguments);

    return std::visit(
        overloaded{
            [&](Identifier& target) -> Object_ptr
            {
                auto resolved_symbol = current_scope->lookup(target.name);
                Doctor::get().fatal_if_nullptr(resolved_symbol, WaspStage::Semantics);

                return evaluate_function_call(call, target, argument_types, resolved_symbol);
            },
            [&](MemberAccess& access) -> Object_ptr
            {
                Object_ptr left_type = visit(access.left);

                if (left_type->is<ClassType_ptr>())
                {
                    auto class_type = left_type->as<ClassType_ptr>();
                    return evaluate_instance_method_call(call, access, argument_types, class_type);
                }
                else if (left_type->is<ModuleType_ptr>())
                {
                    return evaluate_module_function_call(call, access, argument_types);
                }

                Doctor::get().fatal(
                    WaspStage::Semantics,
                    "Cannot invoke method. Receiver is neither a class nor a module."
                );
                return nullptr;
            },
            [&](auto&) -> Object_ptr
            {
                Doctor::get().fatal(
                    WaspStage::Semantics,
                    "Expected an Identifier or MemberAccess as the callable."
                );
                return nullptr;
            }
        },
        call.callable->data
    );
}

Object_ptr SemanticAnalyzer::evaluate_instance_creation(
    Constructor& constructor,
    Identifier& target,
    Symbol_ptr class_symbol,
    const ObjectVector& argument_types
)
{
    target.symbol = class_symbol;

    if (class_symbol->should_be_captured(current_scope->get_closure_depth()))
    {
        target.must_be_captured = true;
    }

    auto class_type_obj = class_symbol->get_type();
    auto class_type = class_type_obj->as<ClassType_ptr>();

    Doctor::get().fatal_if_nullptr(
        class_type,
        WaspStage::Semantics,
        "Constructor target must be a class type."
    );

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

Object_ptr SemanticAnalyzer::evaluate_module_instance_creation(
    Constructor& constructor,
    MemberAccess& access,
    const ObjectVector& argument_types
)
{
    Doctor::get().assert(
        access.left->is<Identifier>() && access.right->is<Identifier>(),
        WaspStage::Semantics,
        "Supports module.ClassName() only."
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
    Symbol_ptr export_symbol = module_data.mod->get_member(method_identifier.name);

    Doctor::get().fatal_if_nullptr(
        export_symbol,
        WaspStage::Semantics,
        "Class '" + method_identifier.name + "' not found in module."
    );

    Doctor::get().assert(
        export_symbol->payload_is<ClassData>(),
        WaspStage::Semantics,
        "Symbol '" + method_identifier.name + "' is not a class."
    );

    access.member_index = module_data.mod->get_member_index(method_identifier.name);

    return evaluate_instance_creation(
        constructor,
        method_identifier,
        export_symbol,
        argument_types
    );
}

Object_ptr SemanticAnalyzer::evaluate_function_call(
    Call& call,
    Identifier& identifier,
    const ObjectVector& argument_types,
    Symbol_ptr overload_symbol
)
{
    identifier.symbol = overload_symbol;

    const auto& function_overloads_data = overload_symbol->get_payload_as<FunctionOverloadsData>();
    const auto overloads = function_overloads_data.get_overloads();

    auto [function, overload_index] = type_checker->get_best_function_signature(
        current_scope,
        overloads,
        argument_types
    );

    if (function->should_be_captured(current_scope->get_closure_depth()))
    {
        identifier.must_be_captured = true;
    }

    if (function->is_native_function_or_method())
    {
        call.overload_index = -1;
    }
    else
    {
        call.overload_index = overload_index;
    }

    return get_function_return_type(function);
}

Object_ptr SemanticAnalyzer::evaluate_instance_method_call(
    Call& call,
    MemberAccess& member_access,
    const ObjectVector& argument_types,
    ClassType_ptr class_type
)
{
    auto method_identifier = member_access.right->try_as<Identifier>();
    auto method_name = method_identifier->name;

    Doctor::get().assert(
        class_type->contains_member(method_name),
        WaspStage::Semantics,
        "Method '" + method_name + "()' does not exist on class " + class_type->name
    );

    auto member = class_type->get_member(method_name);

    Doctor::get().assert(
        member->is<ObjectOverloadList_ptr>(),
        WaspStage::Semantics,
        "Member '" + method_name + "' must be an object overload group"
    );

    const auto object_overloads = member->as<ObjectOverloadList_ptr>();

    ObjectVector valid_matches;
    std::vector<int> match_indices;

    for (size_t i = 0; i < object_overloads->overloads.size(); ++i)
    {
        auto overload = object_overloads->overloads[i];
        auto [return_type, parameter_types] = get_function_signature(overload);

        if (type_checker->assignable(current_scope, parameter_types, argument_types))
        {
            valid_matches.push_back(overload);
            match_indices.push_back(static_cast<int>(i));
        }
    }

    Doctor::get().assert(
        !valid_matches.empty(),
        WaspStage::Semantics,
        "No matching function signature found"
    );

    Doctor::get()
        .assert(valid_matches.size() == 1, WaspStage::Semantics, "Ambiguous function call");

    member_access.member_index = class_type->get_member_index(method_name);

    call.overload_index = match_indices.front();
    call.is_method_call = true;

    return get_function_signature(valid_matches.front()).first;
}

Object_ptr SemanticAnalyzer::evaluate_module_function_call(
    Call& call,
    MemberAccess& access,
    const ObjectVector& argument_types
)
{
    Doctor::get().assert(
        access.left->is<Identifier>() && access.right->is<Identifier>(),
        WaspStage::Semantics,
        "Supports module.function() only."
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
    Symbol_ptr export_symbol = module_data.mod->get_member(method_identifier.name);

    Doctor::get().fatal_if_nullptr(
        export_symbol,
        WaspStage::Semantics,
        "Member '" + method_identifier.name + "' not found in module."
    );

    Doctor::get().assert(
        export_symbol->payload_is<FunctionOverloadsData>(),
        WaspStage::Semantics,
        "Symbol '" + method_identifier.name + "' is not an overload group"
    );

    const auto& group_data = export_symbol->get_payload_as<FunctionOverloadsData>();

    auto [resolved_function, overload_index] = type_checker->get_best_function_signature(
        current_scope,
        group_data.get_overloads(),
        argument_types
    );

    access.member_index = module_data.mod->get_member_index(method_identifier.name);
    call.overload_index = overload_index;
    method_identifier.symbol = resolved_function;

    return get_function_return_type(resolved_function);
}

} // namespace Wasp
