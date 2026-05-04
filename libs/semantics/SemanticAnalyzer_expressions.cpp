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

// ============================================================================
// Call & Instantiation Helpers
// ============================================================================

void SemanticAnalyzer::bind_identifier(Identifier& id, Symbol_ptr symbol)
{
    id.symbol = symbol;
    if (symbol->should_be_captured(current_scope->get_closure_depth()))
    {
        id.must_be_captured = true;
    }
}

Symbol_ptr SemanticAnalyzer::resolve_module_export(MemberAccess& access)
{
    Doctor::get().assert(
        access.left->is<Identifier>() && access.right->is<Identifier>(),
        WaspStage::Semantics,
        "Supports module.member() only."
    );

    auto& module_identifier = access.left->as<Identifier>();
    auto& member_identifier = access.right->as<Identifier>();

    Symbol_ptr module_symbol = current_scope->lookup(module_identifier.name);
    Doctor::get().fatal_if_nullptr(module_symbol, WaspStage::Semantics);

    bind_identifier(module_identifier, module_symbol);

    auto& module_data = module_symbol->get_payload_as<ModuleData>();
    Symbol_ptr export_symbol = module_data.mod->get_member(member_identifier.name);

    Doctor::get().fatal_if_nullptr(
        export_symbol,
        WaspStage::Semantics,
        "Member '" + member_identifier.name + "' not found in module."
    );

    access.member_index = module_data.mod->get_member_index(member_identifier.name);
    return export_symbol;
}

void SemanticAnalyzer::validate_constructor_args(
    ClassType_ptr class_type,
    const ObjectVector& arg_types
)
{
    Doctor::get().assert(
        arg_types.size() == class_type->fields.size(),
        WaspStage::Semantics,
        "Constructor Arguments Count Mismatch. Expected " +
            std::to_string(class_type->fields.size()) + ", got " + std::to_string(arg_types.size())
    );

    for (size_t i = 0; i < class_type->fields.size(); ++i)
    {
        Object_ptr expected_type = class_type->get_member(class_type->fields[i]);
        Doctor::get().assert(
            type_checker->assignable(current_scope, expected_type, arg_types[i]),
            WaspStage::Semantics,
            "Constructor Arguments Type Mismatch for member '" + class_type->fields[i] + "'"
        );
    }
}

// ============================================================================
// Expression Visitors
// ============================================================================

Object_ptr SemanticAnalyzer::visit(Identifier& identifier)
{
    Symbol_ptr resolved_symbol = current_scope->lookup(identifier.name);
    Doctor::get().fatal_if_nullptr(resolved_symbol, WaspStage::Semantics);

    bind_identifier(identifier, resolved_symbol);

    return resolved_symbol->get_type();
}

Object_ptr SemanticAnalyzer::evaluate_function_call(
    Call& call,
    Identifier& identifier,
    const ObjectVector& argument_types,
    Symbol_ptr overload_symbol
)
{
    bind_identifier(identifier, overload_symbol);

    const auto& function_overloads_data = overload_symbol->get_payload_as<OverloadsData>();
    const auto overloads = function_overloads_data.get_overloads();

    auto [function, overload_index] = type_checker->get_best_function_signature(
        current_scope,
        overloads,
        argument_types
    );

    call.overload_index = overload_index;

    return get_function_return_type(function);
}

Object_ptr SemanticAnalyzer::evaluate_module_function_call(
    Call& call,
    MemberAccess& access,
    const ObjectVector& argument_types
)
{
    Symbol_ptr export_symbol = resolve_module_export(access);

    Doctor::get().assert(
        export_symbol->payload_is<OverloadsData>(),
        WaspStage::Semantics,
        "Symbol is not an overload group"
    );

    const auto& group_data = export_symbol->get_payload_as<OverloadsData>();
    auto [resolved_function, overload_index] = type_checker->get_best_function_signature(
        current_scope,
        group_data.get_overloads(),
        argument_types
    );

    call.overload_index = overload_index;
    access.right->as<Identifier>().symbol = resolved_function;

    return get_function_return_type(resolved_function);
}

Object_ptr SemanticAnalyzer::evaluate_template_module_function_call(
    Call& call,
    TemplateInstantiation& template_instantiation,
    MemberAccess& access,
    const ObjectVector& argument_types
)
{
    Symbol_ptr export_symbol = resolve_module_export(access);
    auto& method_identifier = access.right->as<Identifier>();

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
    bind_identifier(target, class_symbol);

    auto class_type = class_symbol->get_type()->as<ClassType_ptr>();
    Doctor::get().fatal_if_nullptr(
        class_type,
        WaspStage::Semantics,
        "Constructor target must be a class type."
    );

    validate_constructor_args(class_type, argument_types);

    return class_symbol->get_type();
}

Object_ptr SemanticAnalyzer::evaluate_module_instance_creation(
    Constructor& constructor,
    MemberAccess& access,
    const ObjectVector& argument_types
)
{
    Symbol_ptr export_symbol = resolve_module_export(access);
    auto& method_identifier = access.right->as<Identifier>();

    Doctor::get().assert(
        export_symbol->payload_is<ClassData>(),
        WaspStage::Semantics,
        "Symbol '" + method_identifier.name + "' is not a class."
    );

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
    auto class_template_type = template_symbol->get_type()->as<TemplateType_ptr>();

    Doctor::get().assert(
        class_template_type->generics.size() == generic_args.size(),
        WaspStage::Semantics,
        "Generic arguments count mismatch."
    );

    size_t arg_idx = 0;
    for (const auto& [name, generic_obj] : class_template_type->generics)
    {
        auto generic_type = generic_obj->as<GenericType_ptr>();
        Doctor::get().assert(
            type_checker
                ->assignable(current_scope, generic_type->constraint_type, generic_args[arg_idx]),
            WaspStage::Semantics,
            "Type bound violated."
        );
        arg_idx++;
    }

    auto base_class_type = class_template_type->underlying_type->as<ClassType_ptr>();

    auto concrete_class_type = std::make_shared<ClassType>(
        base_class_type->name,
        ObjectStringMap{},
        base_class_type->fields,
        base_class_type->methods,
        base_class_type->pures,
        base_class_type->statics
    );
    auto concrete_class_type_obj = make_object(concrete_class_type);

    ObjectStringMap concrete_members;
    for (const auto& [name, type] : base_class_type->members)
    {
        concrete_members[name] = type_checker
                                     ->substitute_generics(type, class_template_type, generic_args);
    }

    auto is_self_type = [&](const Object_ptr& t)
    {
        return (t->is<ClassType_ptr>() && t->as<ClassType_ptr>()->name == base_class_type->name) ||
               (t->is<TemplateType_ptr>() &&
                t->as<TemplateType_ptr>()->underlying_type->as<ClassType_ptr>()->name ==
                    base_class_type->name);
    };

    for (auto& [name, member] : concrete_members)
    {
        if (!member->is<ObjectOverloadList_ptr>())
            continue;

        for (auto& overload : member->as<ObjectOverloadList_ptr>()->overloads)
        {
            if (!overload->is<Signature_ptr>())
                continue;

            auto func = overload->as<Signature_ptr>();

            if (is_self_type(func->return_type))
            {
                func->return_type = concrete_class_type_obj;
            }

            for (auto& param : func->parameter_types)
            {
                if (is_self_type(param))
                {
                    param = concrete_class_type_obj;
                }
            }
        }
    }

    concrete_class_type->members = concrete_members;

    validate_constructor_args(concrete_class_type, argument_types);

    auto concrete_symbol = SymbolFactory::create_class(
        template_symbol->name,
        concrete_class_type_obj,
        template_symbol->closure_depth,
        template_symbol->lexical_depth
    );

    concrete_symbol->id = template_symbol->id;
    template_instantiation.symbol = concrete_symbol;
    template_instantiation.group_symbol = template_symbol;

    bind_identifier(target, concrete_symbol);

    return concrete_class_type_obj;
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
    else if (left_type->is<TemplateType_ptr>())
    {
        // Must handle the fact that underlying_type is an Object_ptr variant now
        const auto type = left_type->as<TemplateType_ptr>()->underlying_type;

        if (type->is<ClassType_ptr>())
        {
            auto class_type = type->as<ClassType_ptr>();
            expr.member_index = class_type->get_member_index(member_name);
            return class_type->get_member(member_name);
        }
        else if (type->is<TraitType_ptr>())
        {
            auto trait_type = type->as<TraitType_ptr>();
            expr.member_index = trait_type->get_member_index(member_name);
            return trait_type->get_member(member_name);
        }
    }
    else if (left_type->is<EnumType_ptr>())
    {
        const auto type = left_type->as<EnumType_ptr>();
        std::string full_name = type->name + "." + member_name;

        if (type->nested_enums.contains(full_name))
        {
            return make_object(type->nested_enums.at(full_name));
        }

        Doctor::get().assert(
            type->members.contains(full_name),
            WaspStage::Semantics,
            "Enum '" + type->name + "' does not contain member '" + member_name + "'."
        );

        expr.member_index = type->members.at(full_name);
        expr.is_enum_value = true;

        return left_type;
    }

    Doctor::get().fatal(
        WaspStage::Semantics,
        "Cannot access member '" + member_name +
            "'. The left-hand side is not a module, class, trait, or enum."
    );
    return nullptr;
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

Object_ptr SemanticAnalyzer::evaluate_type_template_instantiation(
    TemplateInstantiation& template_instantiation,
    Identifier& target,
    Symbol_ptr template_symbol,
    const ObjectVector& generic_args
)
{
    template_instantiation.group_symbol = template_symbol;
    bind_identifier(target, template_symbol);

    auto type_obj = template_symbol->get_type();
    Doctor::get().fatal_if_nullptr(type_obj, WaspStage::Semantics);

    if (type_obj->is<TemplateType_ptr>())
    {
        auto t_type = type_obj->as<TemplateType_ptr>();

        Doctor::get().assert(
            t_type->generics.size() == generic_args.size(),
            WaspStage::Semantics,
            "Generic arguments count mismatch."
        );

        size_t arg_idx = 0;
        for (const auto& [name, generic_obj] : t_type->generics)
        {
            auto generic_type = generic_obj->as<GenericType_ptr>();
            Doctor::get().assert(
                type_checker->assignable(
                    current_scope,
                    generic_type->constraint_type,
                    generic_args[arg_idx]
                ),
                WaspStage::Semantics,
                "Type bound violated for parameter '" + name + "'."
            );
            arg_idx++;
        }

        if (t_type->underlying_type->is<ClassType_ptr>())
        {
            auto base_class_type = t_type->underlying_type->as<ClassType_ptr>();
            ObjectStringMap concrete_members;

            for (const auto& [name, member_type] : base_class_type->members)
            {
                concrete_members[name] = type_checker->substitute_generics(
                    member_type,
                    t_type,
                    generic_args
                );
            }

            return make_object(
                std::make_shared<ClassType>(
                    base_class_type->name,
                    concrete_members,
                    base_class_type->fields,
                    base_class_type->methods,
                    base_class_type->pures,
                    base_class_type->statics
                )
            );
        }
        else if (t_type->underlying_type->is<TraitType_ptr>())
        {
            auto base_trait_type = t_type->underlying_type->as<TraitType_ptr>();
            ObjectStringMap concrete_members;

            for (const auto& [name, member_type] : base_trait_type->members)
            {
                concrete_members[name] = type_checker->substitute_generics(
                    member_type,
                    t_type,
                    generic_args
                );
            }

            return make_object(
                std::make_shared<TraitType>(
                    base_trait_type->name,
                    concrete_members,
                    base_trait_type->methods,
                    base_trait_type->pures,
                    base_trait_type->statics
                )
            );
        }

        // It's a type alias template
        return type_checker->substitute_generics(t_type->underlying_type, t_type, generic_args);
    }

    Doctor::get().fatal(WaspStage::Semantics, "Symbol is not a valid type template.");
    return nullptr;
}

Object_ptr SemanticAnalyzer::visit(TemplateInstantiation& template_instantiation)
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

                return evaluate_type_template_instantiation(
                    template_instantiation,
                    target,
                    template_symbol,
                    generic_args
                );
            },
            [&](MemberAccess& access) -> Object_ptr
            {
                Symbol_ptr export_symbol = resolve_module_export(access);

                return evaluate_type_template_instantiation(
                    template_instantiation,
                    access.right->as<Identifier>(),
                    export_symbol,
                    generic_args
                );
            },
            [&](auto&) -> Object_ptr
            {
                Doctor::get().fatal(
                    WaspStage::Semantics,
                    "Expected an Identifier or MemberAccess inside TemplateInstantiation."
                );
                return nullptr;
            }
        },
        template_instantiation.target->data
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

                ClassType_ptr class_type = nullptr;
                TraitType_ptr trait_type = nullptr;

                // 1. Resolve Underlying Class Types
                if (left_type->is<ClassType_ptr>())
                {
                    class_type = left_type->as<ClassType_ptr>();
                }
                else if (left_type->is<TemplateType_ptr>())
                {
                    if (left_type->as<TemplateType_ptr>()->underlying_type->is<ClassType_ptr>())
                    {
                        class_type = left_type->as<TemplateType_ptr>()
                                         ->underlying_type->as<ClassType_ptr>();
                    }
                }

                // 2. Resolve Underlying Trait Types
                if (!class_type)
                {
                    if (left_type->is<TraitType_ptr>())
                    {
                        trait_type = left_type->as<TraitType_ptr>();
                    }
                    else if (left_type->is<TemplateType_ptr>())
                    {
                        if (left_type->as<TemplateType_ptr>()->underlying_type->is<TraitType_ptr>())
                        {
                            trait_type = left_type->as<TemplateType_ptr>()
                                             ->underlying_type->as<TraitType_ptr>();
                        }
                    }
                }

                // --- Class Method Evaluation ---
                if (class_type)
                {
                    if (access.right->is<Identifier>())
                    {
                        std::string method_name = access.right->as<Identifier>().name;

                        if (!class_type->is_pure(method_name) &&
                            !class_type->is_static(method_name))
                        {
                            call.is_method_call = true;
                        }
                    }

                    return evaluate_class_method_call(call, access, argument_types, class_type);
                }

                // --- Trait Method Evaluation ---
                else if (trait_type)
                {
                    if (access.right->is<Identifier>())
                    {
                        std::string method_name = access.right->as<Identifier>().name;

                        if (!trait_type->is_pure(method_name) &&
                            !trait_type->is_static(method_name))
                        {
                            call.is_method_call = true;
                        }
                    }

                    return evaluate_trait_method_call(call, access, argument_types, trait_type);
                }

                // --- Module Evaluation ---
                else if (left_type->is<ModuleType_ptr>())
                {
                    return evaluate_module_function_call(call, access, argument_types);
                }

                Doctor::get().fatal(
                    WaspStage::Semantics,
                    "Cannot invoke method. Receiver is neither a class, trait, nor a module."
                );
                return nullptr;
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
                return nullptr;
            }
        },
        call.callable->data
    );
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
                return nullptr;
            },
            [&](auto&) -> Object_ptr
            {
                Doctor::get().fatal(
                    WaspStage::Semantics,
                    "Expected an Identifier or MemberAccess inside TemplateInstantiation."
                );
                return nullptr;
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
    Symbol_ptr overload_group_symbol
)
{
    ObjectVector generic_args;

    for (const auto& arg : template_instantiation.arguments)
    {
        generic_args.push_back(visit(arg));
    }

    Doctor::get().assert(
        overload_group_symbol->payload_is<OverloadsData>(),
        WaspStage::Semantics,
        "Expected an overload group for template function call."
    );

    const auto& group_data = overload_group_symbol->get_payload_as<OverloadsData>();
    const auto& overloads = group_data.get_overloads();

    ObjectVector valid_matches;
    std::vector<int> match_indices;
    Object_ptr final_return_type = nullptr;
    Symbol_ptr matched_template_symbol = nullptr;
    ObjectVector final_concrete_param_types;

    for (size_t i = 0; i < overloads.size(); ++i)
    {
        auto overload = overloads[i];
        auto type_obj = overload->get_type();

        auto t = type_obj->try_as<TemplateType_ptr>();
        if (!t)
            continue;

        auto function_template_type = *t;

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
        auto underlying_sig = function_template_type->underlying_type->as<Signature_ptr>();
        for (const auto& param_type : underlying_sig->parameter_types)
        {
            concrete_param_types.push_back(
                type_checker->substitute_generics(param_type, function_template_type, generic_args)
            );
        }

        if (type_checker->assignable(current_scope, concrete_param_types, argument_types))
        {
            valid_matches.push_back(type_obj);
            match_indices.push_back(static_cast<int>(i));
            matched_template_symbol = overload;
            final_concrete_param_types = concrete_param_types;

            final_return_type = type_checker->substitute_generics(
                underlying_sig->return_type,
                function_template_type,
                generic_args
            );
        }
    }

    Doctor::get().assert(
        !valid_matches.empty(),
        WaspStage::Semantics,
        "No matching template function signature found for '" + overload_group_symbol->name + "'"
    );

    Doctor::get().assert(
        valid_matches.size() == 1,
        WaspStage::Semantics,
        "Ambiguous template function call for '" + overload_group_symbol->name + "'"
    );

    auto concrete_function_type = make_object(
        std::make_shared<Signature>(Signature{final_concrete_param_types, final_return_type})
    );

    auto concrete_symbol = SymbolFactory::create_function(
        overload_group_symbol->name,
        concrete_function_type,
        matched_template_symbol->is_native(),
        overload_group_symbol->closure_depth,
        overload_group_symbol->lexical_depth
    );

    concrete_symbol->id = overload_group_symbol->id;

    template_instantiation.symbol = concrete_symbol;
    template_instantiation.group_symbol = overload_group_symbol;
    target.symbol = concrete_symbol;

    if (overload_group_symbol->should_be_captured(current_scope->get_closure_depth()))
    {
        target.must_be_captured = true;
    }

    call.overload_index = matched_template_symbol->is_native_function_or_method()
                              ? -1
                              : match_indices.front();

    return final_return_type;
}

Object_ptr SemanticAnalyzer::evaluate_class_method_call(
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

    Object_ptr final_return_type = get_function_signature(valid_matches.front()).first;

    if (final_return_type->is<TemplateType_ptr>())
    {
        final_return_type = make_object(class_type);
    }

    return final_return_type;
}

Object_ptr SemanticAnalyzer::evaluate_trait_method_call(
    Call& call,
    MemberAccess& member_access,
    const ObjectVector& argument_types,
    TraitType_ptr trait_type
)
{
    auto method_identifier = member_access.right->try_as<Identifier>();
    auto method_name = method_identifier->name;

    Doctor::get().assert(
        trait_type->contains_member(method_name),
        WaspStage::Semantics,
        "Method '" + method_name + "()' does not exist on trait " + trait_type->name
    );

    auto member = trait_type->get_member(method_name);

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

    member_access.member_index = trait_type->get_member_index(method_name);

    call.overload_index = match_indices.front();
    call.is_method_call = true;
    call.is_pure_method_call = trait_type->is_pure(method_name);

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

        if (!overload->is<TemplateType_ptr>())
            continue;
        auto function_template_type = overload->as<TemplateType_ptr>();

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
        auto underlying_sig = function_template_type->underlying_type->as<Signature_ptr>();
        for (const auto& param_type : underlying_sig->parameter_types)
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
                underlying_sig->return_type,
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

} // namespace Wasp
