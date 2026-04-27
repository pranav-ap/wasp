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
    else if (left_type->is<ClassTemplateType_ptr>())
    {
        const auto type = left_type->as<ClassTemplateType_ptr>()->class_type;

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
            [&](TemplateInstantiation& template_instantiation) -> Object_ptr
            {
                ObjectVector generic_args;

                for (const auto& arg : template_instantiation.arguments)
                {
                    generic_args.push_back(visit(arg));
                }

                return std::visit(
                    overloaded{
                        [&](Identifier& target) -> Object_ptr
                        {
                            auto template_symbol = current_scope->lookup(target.name);
                            Doctor::get().fatal_if_nullptr(template_symbol, WaspStage::Semantics);

                            return evaluate_class_template_instantiation(
                                constructor,
                                template_instantiation,
                                target,
                                argument_types,
                                generic_args,
                                template_symbol
                            );
                        },
                        [&](auto&) -> Object_ptr
                        {
                            Doctor::get().fatal(
                                WaspStage::Semantics,
                                "Expected an Identifier for class template target."
                            );
                            return nullptr;
                        }
                    },
                    template_instantiation.target->data
                );
            },
            [&](auto&) -> Object_ptr
            {
                Doctor::get().fatal(
                    WaspStage::Semantics,
                    "Expected an Identifier, MemberAccess, or TemplateInstantiation as the "
                    "constructor target."
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
                    return evaluate_method_call(call, access, argument_types, class_type);
                }
                else if (left_type->is<ModuleType_ptr>())
                {
                    return evaluate_module_function_call(call, access, argument_types);
                }

                Doctor::get().fatal(
                    WaspStage::Semantics,
                    "Cannot invoke method. Receiver is neither a class nor a module."
                );
            },
            [&](TemplateInstantiation& template_instantiation) -> Object_ptr
            {
                return evaluate_template_call(call, template_instantiation, argument_types);
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

Object_ptr SemanticAnalyzer::evaluate_template_call(
    Call& call,
    TemplateInstantiation& template_instantiation,
    const ObjectVector& argument_types
)
{
    ObjectVector generic_args;

    for (const auto& arg : template_instantiation.arguments)
    {
        generic_args.push_back(visit(arg));
    }

    return std::visit(
        overloaded{
            [&](Identifier& target) -> Object_ptr
            {
                auto overload_symbol = current_scope->lookup(target.name);
                Doctor::get().fatal_if_nullptr(overload_symbol, WaspStage::Semantics);

                return evaluate_template_function_call(
                    call,
                    template_instantiation,
                    target,
                    argument_types,
                    overload_symbol
                );
            },
            [&](MemberAccess& access) -> Object_ptr
            {
                Object_ptr left_type = visit(access.left);

                if (left_type->is<ClassType_ptr>())
                {
                    auto class_type = left_type->as<ClassType_ptr>();

                    return evaluate_template_method_call(
                        call,
                        template_instantiation,
                        access,
                        argument_types,
                        class_type
                    );
                }
                else if (left_type->is<ModuleType_ptr>())
                {
                    return evaluate_template_module_function_call(
                        call,
                        template_instantiation,
                        access,
                        argument_types
                    );
                }

                Doctor::get().fatal(
                    WaspStage::Semantics,
                    "Cannot invoke template method. Receiver is neither a class nor a module."
                );
            },
            [&](auto&) -> Object_ptr
            {
                Doctor::get().fatal(
                    WaspStage::Semantics,
                    "Expected an Identifier or MemberAccess inside TemplateInstantiation."
                );
            }
        },
        template_instantiation.target->data
    );
}

Object_ptr SemanticAnalyzer::evaluate_template_function_call(
    Call& call,
    TemplateInstantiation& template_instantiation,
    Identifier& target,
    const ObjectVector& argument_types,
    Symbol_ptr overload_symbol
)
{
    ObjectVector generic_args;

    for (const auto& arg : template_instantiation.arguments)
    {
        generic_args.push_back(visit(arg));
    }

    auto function_template_type = overload_symbol->get_type()->as<FunctionTemplateType_ptr>();

    Doctor::get().assert(
        function_template_type->generics.size() == generic_args.size(),
        WaspStage::Semantics,
        "Generic arguments count mismatch. Expected " +
            std::to_string(function_template_type->generics.size()) + ", got " +
            std::to_string(generic_args.size())
    );

    size_t arg_idx = 0;

    for (const auto& [name, generic_obj] : function_template_type->generics)
    {
        auto generic_type = generic_obj->as<GenericType_ptr>();

        Doctor::get().assert(
            type_checker
                ->assignable(current_scope, generic_type->constraint_type, generic_args[arg_idx]),
            WaspStage::Semantics,
            "Type bound violated for generic parameter " + name
        );

        arg_idx++;
    }

    ObjectVector concrete_param_types;
    for (const auto& param_type : function_template_type->signature->parameter_types)
    {
        concrete_param_types.push_back(
            type_checker->substitute_generics(param_type, function_template_type, generic_args)
        );
    }

    Object_ptr concrete_return_type = type_checker->substitute_generics(
        function_template_type->signature->return_type,
        function_template_type,
        generic_args
    );

    Doctor::get().assert(
        type_checker->assignable(current_scope, concrete_param_types, argument_types),
        WaspStage::Semantics,
        "Arguments do not match the instantiated generic function parameters."
    );

    auto concrete_function_type = make_object(
        std::make_shared<FunctionType>(concrete_param_types, concrete_return_type)
    );

    auto concrete_symbol = SymbolFactory::create_function(
        overload_symbol->name,
        concrete_function_type,
        false,
        overload_symbol->closure_depth,
        overload_symbol->lexical_depth
    );

    concrete_symbol->id = overload_symbol->id;

    template_instantiation.symbol = concrete_symbol;
    template_instantiation.group_symbol = overload_symbol;
    target.symbol = concrete_symbol;

    if (overload_symbol->should_be_captured(current_scope->get_closure_depth()))
    {
        target.must_be_captured = true;
    }

    call.overload_index = concrete_symbol->is_native_function_or_method() ? -1 : 0;

    return concrete_return_type;
}

Object_ptr SemanticAnalyzer::evaluate_method_call(
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
    call.is_pure_method_call = class_type->is_pure(method_name);

    return get_function_signature(valid_matches.front()).first;
}

Object_ptr SemanticAnalyzer::evaluate_template_method_call(
    Call& call,
    TemplateInstantiation& template_instantiation,
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

    ObjectVector generic_args;
    for (const auto& arg : template_instantiation.arguments)
    {
        generic_args.push_back(visit(arg));
    }

    const auto object_overloads = member->as<ObjectOverloadList_ptr>();

    ObjectVector valid_matches;
    std::vector<int> match_indices;
    Object_ptr final_return_type = nullptr;

    for (size_t i = 0; i < object_overloads->overloads.size(); ++i)
    {
        auto overload = object_overloads->overloads[i];

        if (!overload->is<FunctionTemplateType_ptr>())
            continue;
        auto function_template_type = overload->as<FunctionTemplateType_ptr>();

        if (function_template_type->generics.size() != generic_args.size())
            continue;

        bool bounds_met = true;
        size_t arg_idx = 0;

        for (const auto& [name, generic_obj] : function_template_type->generics)
        {
            auto generic_type = generic_obj->as<GenericType_ptr>();
            if (!type_checker->assignable(
                    current_scope,
                    generic_type->constraint_type,
                    generic_args[arg_idx]
                ))
            {
                bounds_met = false;
                break;
            }
            arg_idx++;
        }

        if (!bounds_met)
            continue;

        ObjectVector concrete_param_types;
        for (const auto& param_type : function_template_type->signature->parameter_types)
        {
            concrete_param_types.push_back(
                type_checker->substitute_generics(param_type, function_template_type, generic_args)
            );
        }

        if (type_checker->assignable(current_scope, concrete_param_types, argument_types))
        {
            valid_matches.push_back(overload);
            match_indices.push_back(static_cast<int>(i));

            final_return_type = type_checker->substitute_generics(
                function_template_type->signature->return_type,
                function_template_type,
                generic_args
            );
        }
    }

    Doctor::get().assert(
        !valid_matches.empty(),
        WaspStage::Semantics,
        "No matching generic method signature found"
    );

    Doctor::get()
        .assert(valid_matches.size() == 1, WaspStage::Semantics, "Ambiguous generic method call");

    member_access.member_index = class_type->get_member_index(method_name);

    call.overload_index = match_indices.front();
    call.is_method_call = true;
    call.is_pure_method_call = class_type->is_pure(method_name);

    return final_return_type;
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

Object_ptr SemanticAnalyzer::evaluate_template_module_function_call(
    Call& call,
    TemplateInstantiation& template_instantiation,
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

    access.member_index = module_data.mod->get_member_index(method_identifier.name);

    return evaluate_template_function_call(
        call,
        template_instantiation,
        method_identifier,
        argument_types,
        export_symbol
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

Object_ptr SemanticAnalyzer::evaluate_class_template_instantiation(
    Constructor& constructor,
    TemplateInstantiation& template_instantiation,
    Identifier& target,
    const ObjectVector& argument_types,
    const ObjectVector& generic_args,
    Symbol_ptr template_symbol
)
{
    auto template_type_obj = template_symbol->get_type();
    auto class_template_type = template_type_obj->as<ClassTemplateType_ptr>();

    Doctor::get().assert(
        class_template_type->generics.size() == generic_args.size(),
        WaspStage::Semantics,
        "Generic arguments count mismatch. Expected " +
            std::to_string(class_template_type->generics.size()) + ", got " +
            std::to_string(generic_args.size())
    );

    size_t arg_idx = 0;
    for (const auto& [name, generic_obj] : class_template_type->generics)
    {
        auto generic_type = generic_obj->as<GenericType_ptr>();

        Doctor::get().assert(
            type_checker
                ->assignable(current_scope, generic_type->constraint_type, generic_args[arg_idx]),
            WaspStage::Semantics,
            "Type bound violated for generic parameter '" + name + "'."
        );

        arg_idx++;
    }

    auto base_class_type = class_template_type->class_type;

    Doctor::get().assert(
        argument_types.size() == base_class_type->fields.size(),
        WaspStage::Semantics,
        "Constructor Arguments Count Mismatch. Expected " +
            std::to_string(base_class_type->fields.size()) + ", got " +
            std::to_string(argument_types.size())
    );

    ObjectStringMap concrete_members;
    for (const auto& [name, type] : base_class_type->members)
    {
        concrete_members[name] = type_checker
                                     ->substitute_generics(type, class_template_type, generic_args);
    }

    auto concrete_class_type = std::make_shared<ClassType>(
        base_class_type->name,
        concrete_members,
        base_class_type->fields,
        base_class_type->methods,
        base_class_type->pures,
        base_class_type->statics
    );

    auto concrete_class_type_obj = make_object(concrete_class_type);

    for (size_t i = 0; i < concrete_class_type->fields.size(); ++i)
    {
        Object_ptr expected_type = concrete_class_type->get_member(concrete_class_type->fields[i]);
        Object_ptr actual_type = argument_types[i];

        Doctor::get().assert(
            type_checker->assignable(current_scope, expected_type, actual_type),
            WaspStage::Semantics,
            "Constructor Arguments Type Mismatch for member '" + concrete_class_type->fields[i] +
                "'"
        );
    }

    auto concrete_symbol = SymbolFactory::create_class(
        template_symbol->name,
        concrete_class_type_obj,
        template_symbol->closure_depth,
        template_symbol->lexical_depth
    );

    concrete_symbol->id = template_symbol->id;

    template_instantiation.symbol = concrete_symbol;
    template_instantiation.group_symbol = template_symbol;
    target.symbol = concrete_symbol;

    if (template_symbol->should_be_captured(current_scope->get_closure_depth()))
    {
        target.must_be_captured = true;
    }

    return concrete_class_type_obj;
}

} // namespace Wasp
