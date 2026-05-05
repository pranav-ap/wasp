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
#include <utility>
#include <variant>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

// ============================================================================
// Helpers
// ============================================================================

void SemanticAnalyzer::bind_identifier(Identifier& id, Symbol_ptr symbol)
{
    id.symbol = symbol;

    if (symbol->should_be_captured(current_scope->get_closure_depth()))
    {
        id.must_be_captured = true;
    }
}

std::pair<Symbol_ptr, Symbol_ptr> SemanticAnalyzer::get_module_member_symbol(
    MemberAccess& access
)
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
    Symbol_ptr member_symbol = module_data.mod->get_member(member_identifier.name);

    Doctor::get().fatal_if_nullptr(
        member_symbol,
        WaspStage::Semantics,
        "Member '" + member_identifier.name + "' not found in module."
    );

    access.member_index = module_data.mod->get_member_index(member_identifier.name);

    return {module_symbol, member_symbol};
}

// ============================================================================
// Identifiers & Member Access
// ============================================================================

Object_ptr SemanticAnalyzer::visit(Identifier& identifier)
{
    Symbol_ptr symbol = current_scope->lookup(identifier.name);
    Doctor::get().fatal_if_nullptr(symbol, WaspStage::Semantics);

    bind_identifier(identifier, symbol);
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

    return std::visit(
        overloaded{
            [&](ModuleType_ptr type) -> Object_ptr
            {
                expr.member_index = type->get_member_index(member_name);
                return type->get_member(member_name);
            },
            [&](ClassType_ptr type) -> Object_ptr
            {
                expr.member_index = type->get_member_index(member_name);
                return type->get_member(member_name);
            },
            [&](EnumType_ptr type) -> Object_ptr
            {
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
            },
            [&](auto&) -> Object_ptr
            {
                Doctor::get().fatal(
                    WaspStage::Semantics,
                    "Cannot access member '" + member_name +
                        "'. LHS is not a module, class, trait, or enum."
                );
            }
        },
        left_type->value
    );
}

// ============================================================================
// Function Calls & Method Evaluation
// ============================================================================

Object_ptr SemanticAnalyzer::visit(Call& call)
{
    ObjectVector argument_types = visit(call.arguments);

    return std::visit(
        overloaded{
            [&](Identifier& identifier) -> Object_ptr
            {
                auto overload_symbol = current_scope->lookup(identifier.name);

                Doctor::get().fatal_if_nullptr(
                    overload_symbol,
                    WaspStage::Semantics
                );

                return evaluate_function_call(
                    call,
                    identifier,
                    argument_types,
                    overload_symbol
                );
            },
            [&](MemberAccess& access) -> Object_ptr
            {
                Object_ptr left_type = visit(access.left);

                return std::visit(
                    overloaded{
                        [&](ModuleType_ptr const& module_type) -> Object_ptr
                        {
                            auto
                                [module_symbol,
                                 member_symbol] = get_module_member_symbol(access);

                            Doctor::get().assert(
                                member_symbol->payload_is<OverloadsData>(),
                                WaspStage::Semantics,
                                "Module member symbol must hold function overloads."
                            );

                            Doctor::get().assert(
                                access.right->is<Identifier>(),
                                WaspStage::Semantics,
                                "Expected an identifier on the RHS"
                            );

                            return evaluate_function_call(
                                call,
                                access.right->as<Identifier>(),
                                argument_types,
                                member_symbol,
                                module_symbol
                            );
                        },
                        [&](ClassType_ptr const& class_type) -> Object_ptr
                        {
                            return evaluate_method_call(
                                call,
                                access,
                                argument_types,
                                class_type
                            );
                        },
                        [&](const auto&) -> Object_ptr
                        {
                            Doctor::get().fatal(
                                WaspStage::Semantics,
                                "LHS of call must be a module or class instance."
                            );
                        }
                    },
                    left_type->value
                );
            },
            [&](ConcreteTemplate& concrete_template) -> Object_ptr
            {
                ObjectVector concrete_arguments;

                for (const auto& concrete_type : concrete_template.concrete_types)
                {
                    concrete_arguments.push_back(visit(concrete_type));
                }

                return std::visit(
                    overloaded{
                        [&](Identifier& identifier) -> Object_ptr
                        {
                            auto overload_symbol = current_scope->lookup(
                                identifier.name
                            );

                            Doctor::get().fatal_if_nullptr(
                                overload_symbol,
                                WaspStage::Semantics
                            );

                            return evaluate_template_function_call(
                                call,
                                identifier,
                                concrete_arguments,
                                argument_types,
                                overload_symbol
                            );
                        },

                        [&](auto&) -> Object_ptr
                        {
                            Doctor::get().fatal(WaspStage::Semantics, "Boom!");
                        }
                    },
                    concrete_template.target->data
                );
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
    Symbol_ptr overload_symbol,
    Symbol_ptr module_symbol
)
{
    bind_identifier(identifier, overload_symbol);

    auto [function_symbol, index] = type_system->get_best_function_symbol(
        current_scope,
        overload_symbol->get_payload_as<OverloadsData>().get_overloads(),
        argument_types
    );

    call.overload_index = index;

    if (module_symbol)
    {
        identifier.symbol = module_symbol;
    }

    return function_symbol->get_type()->as<Signature_ptr>()->return_type;
}

Object_ptr SemanticAnalyzer::evaluate_method_call(
    Call& call,
    MemberAccess& access,
    const ObjectVector& argument_types,
    ClassType_ptr class_type
)
{
    auto method_name = access.right->as<Identifier>().name;

    Doctor::get().assert(
        class_type->contains_member(method_name),
        WaspStage::Semantics,
        "Method '" + method_name + "()' does not exist on class '" +
            class_type->name + "'."
    );

    auto member = class_type->get_member(method_name);

    Doctor::get().assert(
        member->is<ObjectOverloadList_ptr>(),
        WaspStage::Semantics,
        "Member '" + method_name + "' must be an object overload group."
    );

    const auto& overloads = member->as<ObjectOverloadList_ptr>()->overloads;

    auto [signature_obj, overload_index] = type_system->get_best_function_object(
        current_scope,
        overloads,
        argument_types
    );

    access.member_index = class_type->get_member_index(method_name);
    call.overload_index = overload_index;

    call.is_method_call = true;
    call.is_pure_method_call = class_type->is_pure(method_name);

    return signature_obj->as<Signature_ptr>()->return_type;
}

Object_ptr SemanticAnalyzer::evaluate_template_function_call(
    Call& call,
    Identifier& identifier,
    const ObjectVector& concrete_arguments,
    const ObjectVector& argument_types,
    Symbol_ptr overload_symbol
)
{
    bind_identifier(identifier, overload_symbol);

    auto [function_symbol, index] = type_system->get_best_function_symbol(
        current_scope,
        overload_symbol->get_payload_as<OverloadsData>().get_overloads(),
        argument_types
    );

    call.overload_index = index;

    std::string full_mangled_name = identifier.name + "<" +
                                    mangle_object(concrete_arguments) + ">";

    auto return_type = function_symbol->get_type()->as<Signature_ptr>()->return_type;

    auto concrete_return_type = type_system->substitute_generics(
        return_type,
        concrete_arguments
    );

    return concrete_return_type;
}

// ============================================================================
// Constructor & Instance Creation
// ============================================================================

Object_ptr SemanticAnalyzer::visit(Constructor& constructor)
{
    ObjectVector argument_types = visit(constructor.values);

    return std::visit(
        overloaded{
            [&](Identifier& target) -> Object_ptr
            {
                auto class_symbol = current_scope->lookup(target.name);
                Doctor::get().fatal_if_nullptr(class_symbol, WaspStage::Semantics);

                Doctor::get().assert(
                    class_symbol->payload_is<ClassData>(),
                    WaspStage::Semantics,
                    "Target must be a class."
                );

                return evaluate_instance_creation(
                    constructor,
                    target,
                    class_symbol,
                    argument_types
                );
            },
            [&](MemberAccess& access) -> Object_ptr
            {
                auto [module_symbol, class_symbol] = get_module_member_symbol(
                    access
                );

                Doctor::get().assert(
                    class_symbol->payload_is<ClassData>(),
                    WaspStage::Semantics,
                    "Symbol is not a class."
                );

                Doctor::get().assert(
                    access.right->is<Identifier>(),
                    WaspStage::Semantics,
                    "Expected an identifier on the RHS"
                );

                return evaluate_instance_creation(
                    constructor,
                    access.right->as<Identifier>(),
                    class_symbol,
                    argument_types
                );
            },
            [&](ConcreteTemplate& tc) -> Object_ptr
            {
                Doctor::get().fatal(WaspStage::Semantics, "Boom!");
            },
            [&](auto&) -> Object_ptr
            {
                Doctor::get().fatal(
                    WaspStage::Semantics,
                    "Invalid constructor target"
                );
            }
        },
        constructor.construtable->data
    );
}

Object_ptr SemanticAnalyzer::evaluate_instance_creation(
    Constructor& constructor,
    Identifier& identifier,
    Symbol_ptr class_symbol,
    const ObjectVector& argument_types
)
{
    bind_identifier(identifier, class_symbol);
    auto class_type = class_symbol->get_type()->as<ClassType_ptr>();

    Doctor::get().assert(
        argument_types.size() == class_type->fields.size(),
        WaspStage::Semantics,
        "Constructor Arguments Count Mismatch."
    );

    for (size_t i = 0; i < class_type->fields.size(); ++i)
    {
        Object_ptr expected = class_type->get_member(class_type->fields[i]);

        Doctor::get().assert(
            type_system->assignable(current_scope, expected, argument_types[i]),
            WaspStage::Semantics,
            "Constructor Argument Type Mismatch for field " + class_type->fields[i]
        );
    }

    return class_symbol->get_type();
}

// ============================================================================
// Template Instantiation & Evaluation
// ============================================================================

Object_ptr SemanticAnalyzer::visit(ConcreteTemplate& concrete_template)
{
    Doctor::get().fatal(
        WaspStage::Semantics,
        "ConcreteTemplate is not meant to be visited directly."
    );
}

} // namespace Wasp
