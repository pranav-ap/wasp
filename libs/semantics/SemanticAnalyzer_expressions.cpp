#include "AST.h"
#include "Doctor.h"
#include "Expression.h"
#include "Objects.h"
#include "SemanticAnalyzer.h"
#include "Statement.h"
#include "SymbolScope.h"
#include "Token.h"
#include "Workspace.h"

#include <cctype>
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

// ============================================================================
// Main Visitor Dispatcher
// ============================================================================

Object_ptr SemanticAnalyzer::visit(const Expression_ptr expr)
{
    Doctor::get().fatal_if_nullptr(expr, WaspStage::Semantics);

    return std::visit(
        [&](auto& node) -> Object_ptr
        {
            if constexpr (requires { this->visit(node); })
            {
                return this->visit(node);
            }
            else
            {
                Doctor::get().fatal(
                    WaspStage::Semantics,
                    "Unhandled Expression in Semantic Analyzer"
                );
            }
        },
        expr->data
    );
}

void SemanticAnalyzer::visit(ExpressionStatement& statement)
{
    visit(statement.expression);
}

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

// ============================================================================
// Primitives & Operators
// ============================================================================

Object_ptr SemanticAnalyzer::visit(IntegerLiteral& expr)
{
    Symbol_ptr symbol = current_scope->lookup("int");

    Doctor::get().fatal_if_nullptr(
        symbol,
        WaspStage::Semantics,
        "Type 'int' not found in scope."
    );

    return symbol->get_type();
}

Object_ptr SemanticAnalyzer::visit(FloatLiteral& expr)
{
    Symbol_ptr symbol = current_scope->lookup("float");

    Doctor::get().fatal_if_nullptr(
        symbol,
        WaspStage::Semantics,
        "Type 'float' not found in scope."
    );

    return symbol->get_type();
}

Object_ptr SemanticAnalyzer::visit(StringLiteral& expr)
{
    Symbol_ptr symbol = current_scope->lookup("str");

    Doctor::get().fatal_if_nullptr(
        symbol,
        WaspStage::Semantics,
        "Type 'string' not found in scope."
    );

    return symbol->get_type();
}

Object_ptr SemanticAnalyzer::visit(BooleanLiteral& expr)
{
    Symbol_ptr symbol = current_scope->lookup("bool");

    Doctor::get().fatal_if_nullptr(
        symbol,
        WaspStage::Semantics,
        "Type 'bool' not found in scope."
    );

    return symbol->get_type();
}

Object_ptr SemanticAnalyzer::visit(NoneLiteral& expr)
{
    return workspace->pool->get_none_type();
}

Object_ptr SemanticAnalyzer::visit(DotLiteral& expr)
{
    Doctor::get().fatal(
        WaspStage::Semantics,
        "Dot Literals are not supported yet"
    );
}

Object_ptr SemanticAnalyzer::visit(Prefix& expr)
{
    Object_ptr operand_type = visit(expr.operand);

    if (is_native_type(operand_type))
    {
        return type_system->infer(current_scope, operand_type, expr.op.type);
    }

    std::string operator_name = to_string(expr.op.type);
    Symbol_ptr operator_symbol = current_scope->lookup(operator_name);

    Doctor::get().fatal_if_nullptr(
        operator_symbol,
        WaspStage::Semantics,
        "Undefined operator '" + operator_name + "'"
    );

    Doctor::get().assert(
        operator_symbol->payload_is<OverloadsData>(),
        WaspStage::Semantics,
        "Operator '" + operator_name +
            "' is not overloaded but used as an operator."
    );

    auto overloads = operator_symbol->get_payload_as<OverloadsData>()
                         .get_overloads();

    auto [function_symbol, overload_index] = type_system
                                                 ->get_best_function_symbol(
                                                     current_scope,
                                                     overloads,
                                                     {operand_type}
                                                 );

    expr.symbol = operator_symbol;
    expr.overload_index = overload_index;

    return function_symbol->get_type()->as<Signature_ptr>()->return_type;
}

Object_ptr SemanticAnalyzer::visit(Infix& expr)
{
    Object_ptr left_type = visit(expr.left);
    Object_ptr right_type = visit(expr.right);

    if (is_native_type(left_type) && is_native_type(right_type))
    {
        return type_system
            ->infer(current_scope, left_type, expr.op.type, right_type);
    }

    std::string operator_name = to_string(expr.op.type);
    Symbol_ptr operator_symbol = current_scope->lookup(operator_name)
                                     ->resolve();

    Doctor::get().fatal_if_nullptr(
        operator_symbol,
        WaspStage::Semantics,
        "Undefined operator '" + operator_name + "' for non-native types."
    );

    Doctor::get().assert(
        operator_symbol->payload_is<OverloadsData>(),
        WaspStage::Semantics,
        "Operator '" + operator_name +
            "' is not overloaded but used as an operator."
    );

    auto overloads = operator_symbol->get_payload_as<OverloadsData>()
                         .get_overloads();

    auto [function_symbol, overload_index] = type_system
                                                 ->get_best_function_symbol(
                                                     current_scope,
                                                     overloads,
                                                     {left_type, right_type}
                                                 );

    expr.symbol = operator_symbol;
    expr.overload_index = overload_index;

    return function_symbol->get_type()->as<Signature_ptr>()->return_type;
}

Object_ptr SemanticAnalyzer::visit(Postfix& expr)
{
    Object_ptr operand_type = visit(expr.operand);

    if (is_native_type(operand_type))
    {
        type_system->expect_number_type(operand_type);
        return operand_type;
    }

    std::string operator_name = to_string(expr.op.type);
    Symbol_ptr operator_symbol = current_scope->lookup(operator_name);

    Doctor::get().fatal_if_nullptr(
        operator_symbol,
        WaspStage::Semantics,
        "Undefined operator '" + operator_name + "' for non-native types."
    );

    Doctor::get().assert(
        operator_symbol->payload_is<OverloadsData>(),
        WaspStage::Semantics,
        "Operator '" + operator_name +
            "' is not overloaded but used as an operator."
    );

    auto overloads = operator_symbol->get_payload_as<OverloadsData>()
                         .get_overloads();

    auto [function_symbol, overload_index] = type_system
                                                 ->get_best_function_symbol(
                                                     current_scope,
                                                     overloads,
                                                     {operand_type}
                                                 );

    expr.symbol = operator_symbol;
    expr.overload_index = overload_index;

    return function_symbol->get_type()->as<Signature_ptr>()->return_type;
}

// ============================================================================
// Collections
// ============================================================================

Object_ptr SemanticAnalyzer::visit(ListLiteral& expr)
{
    if (expr.expressions.empty())
    {
        return make_object(ListType(make_object(NativeAnyType())));
    }

    ObjectVector element_types;
    for (const auto& element : expr.expressions)
    {
        element_types.push_back(visit(element));
    }

    ObjectVector unique_types = type_system->remove_duplicates(
        current_scope,
        element_types
    );
    if (unique_types.size() == 1)
    {
        return make_object(ListType(unique_types[0]));
    }
    return make_object(ListType(make_object(VariantType(unique_types))));
}

Object_ptr SemanticAnalyzer::visit(TupleLiteral& expr)
{
    ObjectVector element_types;
    for (const auto& element : expr.expressions)
    {
        element_types.push_back(visit(element));
    }
    return make_object(TupleType(element_types));
}

Object_ptr SemanticAnalyzer::visit(MapLiteral& expr)
{
    if (expr.pairs.empty())
    {
        return make_object(
            MapType(make_object(NativeAnyType()), make_object(NativeAnyType()))
        );
    }

    ObjectVector key_types, val_types;
    for (const auto& [k_expr, v_expr] : expr.pairs)
    {
        Object_ptr k_type = visit(k_expr);
        type_system->expect_key_type(current_scope, k_type);
        key_types.push_back(k_type);
        val_types.push_back(visit(v_expr));
    }

    ObjectVector u_keys = type_system->remove_duplicates(
        current_scope,
        key_types
    );
    ObjectVector u_vals = type_system->remove_duplicates(
        current_scope,
        val_types
    );

    Object_ptr fk = u_keys.size() == 1 ? u_keys[0]
                                       : make_object(VariantType(u_keys));
    Object_ptr fv = u_vals.size() == 1 ? u_vals[0]
                                       : make_object(VariantType(u_vals));
    return make_object(MapType(fk, fv));
}

Object_ptr SemanticAnalyzer::visit(SetLiteral& expr)
{
    if (expr.expressions.empty())
    {
        return make_object(SetType(make_object(NativeAnyType())));
    }

    ObjectVector element_types;
    for (const auto& element : expr.expressions)
    {
        Object_ptr el_type = visit(element);
        type_system->expect_key_type(current_scope, el_type);
        element_types.push_back(el_type);
    }

    ObjectVector unique_types = type_system->remove_duplicates(
        current_scope,
        element_types
    );
    if (unique_types.size() == 1)
    {
        return make_object(SetType(unique_types[0]));
    }
    return make_object(SetType(make_object(VariantType(unique_types))));
}

Object_ptr SemanticAnalyzer::visit(RangeLiteral& expr)
{
    Object_ptr start_type = expr.start ? visit(expr.start) : nullptr;
    Object_ptr end_type = expr.end ? visit(expr.end) : nullptr;

    if (start_type)
    {
        type_system->expect_number_type(start_type);
    }

    if (end_type)
    {
        type_system->expect_number_type(end_type);
    }

    if (type_system->is_float_type(start_type) ||
        type_system->is_float_type(end_type))
    {
        return make_object(ListType(make_object(FloatType())));
    }

    return make_object(ListType(make_object(IntType())));
}

Object_ptr SemanticAnalyzer::visit(TypePattern& expr)
{
    Doctor::get().fatal(
        WaspStage::Semantics,
        "Type patterns can only be used in match patterns."
    );
}

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

Symbol_ptr SemanticAnalyzer::get_module_member_symbol(MemberAccess& access)
{
    Doctor::get().assert(
        access.left->is<Identifier>() && access.right->is<Identifier>(),
        WaspStage::Semantics,
        "Supports module.member() only."
    );

    auto& module_identifier = access.left->as<Identifier>();
    auto& member_identifier = access.right->as<Identifier>();

    Symbol_ptr unresolved_module_symbol = current_scope->lookup(
        module_identifier.name
    );
    Doctor::get().fatal_if_nullptr(
        unresolved_module_symbol,
        WaspStage::Semantics
    );

    bind_identifier(module_identifier, unresolved_module_symbol);

    Symbol_ptr resolved_module_symbol = unresolved_module_symbol->resolve();
    auto& module_data = resolved_module_symbol->get_payload_as<ModuleData>();

    Symbol_ptr member_symbol = module_data.mod->get_member(
        member_identifier.name
    );

    Doctor::get().fatal_if_nullptr(
        member_symbol,
        WaspStage::Semantics,
        "Member '" + member_identifier.name + "' not found in module."
    );

    access.member_index = module_data.mod->get_member_index(
        member_identifier.name
    );

    return member_symbol; // Now just returns the single pointer
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
                    "Enum '" + type->name + "' does not contain member '" +
                        member_name + "'."
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

Object_ptr SemanticAnalyzer::visit(TemplateAngular& node)
{
    ObjectVector angular_arguments;
    for (const auto& arg_node : node.angular_nodes)
    {
        angular_arguments.push_back(visit(arg_node));
    }

    Symbol_ptr target_symbol = resolve_target_symbol(node.target);
    node.symbol = target_symbol;

    if (target_symbol->payload_is<ClassData>())
    {
        Object_ptr base = target_symbol->get_type();
        auto names = type_system->get_generics_declaration_order(base);

        Doctor::get().assert(
            names.size() == angular_arguments.size(),
            WaspStage::Semantics,
            "Generic argument count mismatch. Expected " +
                std::to_string(names.size()) + ", got " +
                std::to_string(angular_arguments.size()) + "."
        );

        ObjectStringMap substitutions;
        for (size_t i = 0; i < angular_arguments.size(); ++i)
        {
            substitutions[names[i]] = angular_arguments[i];
        }

        return type_system->substitute_generics(base, substitutions);
    }

    if (target_symbol->payload_is<OverloadsData>())
    {
        auto candidates = target_symbol->get_payload_as<OverloadsData>()
                              .get_overloads_with_indices();

        auto result = type_system->specialize_candidates(
            candidates,
            angular_arguments
        );

        Doctor::get().assert(
            result.signatures.size() == 1,
            WaspStage::Semantics,
            "Ambiguous generic reference for '" + target_symbol->name + "'."
        );

        node.overload_index = result.original_indices[0];
        return result.signatures[0];
    }

    Doctor::get().fatal(
        WaspStage::Semantics,
        "Target '" + target_symbol->name +
            "' does not support template angulars."
    );
}

// ============================================================================
// Calls
// ============================================================================

Object_ptr SemanticAnalyzer::visit(Call& call)
{
    ObjectVector argument_types = visit(call.arguments);

    return std::visit(
        overloaded{
            [&](Identifier& identifier) -> Object_ptr
            {
                auto symbol = current_scope->lookup(identifier.name)->resolve();
                Doctor::get().fatal_if_nullptr(symbol, WaspStage::Semantics);
                bind_identifier(identifier, symbol);

                return resolve_standard_overload(call, symbol, argument_types);
            },
            [&](TemplateAngular& concrete_template) -> Object_ptr
            {
                return call_concrete_template(
                    call,
                    concrete_template,
                    argument_types
                );
            },
            [&](MemberAccess& access) -> Object_ptr
            {
                Object_ptr left_type = visit(access.left);

                return std::visit(
                    overloaded{
                        [&](ClassType_ptr const& class_type) -> Object_ptr
                        {
                            return call_method(
                                call,
                                access,
                                argument_types,
                                class_type
                            );
                        },

                        [&](ModuleType_ptr const& module_type) -> Object_ptr
                        {
                            auto member_symbol = get_module_member_symbol(
                                access
                            );
                            access.right->as<Identifier>()
                                .symbol = member_symbol;

                            return resolve_standard_overload(
                                call,
                                member_symbol,
                                argument_types
                            );
                        },

                        [&](const auto&) -> Object_ptr
                        {
                            Doctor::get().fatal(
                                WaspStage::Semantics,
                                "LHS of call must be a module or class type."
                            );
                        }
                    },
                    left_type->value
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

Object_ptr SemanticAnalyzer::resolve_standard_overload(
    Call& call,
    Symbol_ptr overload_symbol,
    const ObjectVector& argument_types
)
{
    Doctor::get().assert(
        overload_symbol->payload_is<OverloadsData>(),
        WaspStage::Semantics,
        "Symbol must hold function overloads."
    );

    auto [function_symbol, overload_index] = type_system
                                                 ->get_best_function_symbol(
                                                     current_scope,
                                                     overload_symbol
                                                         ->get_payload_as<
                                                             OverloadsData>()
                                                         .get_overloads(),
                                                     argument_types
                                                 );

    call.overload_index = overload_index;
    return function_symbol->get_type()->as<Signature_ptr>()->return_type;
}

Object_ptr SemanticAnalyzer::call_method(
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

    auto [signature_obj, overload_index] = type_system
                                               ->get_best_function_object(
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

Symbol_ptr SemanticAnalyzer::resolve_target_symbol(Expression_ptr target)
{
    Symbol_ptr target_symbol = nullptr;

    std::visit(
        overloaded{
            [&](Identifier& id)
            {
                target_symbol = current_scope->lookup(id.name)->resolve();
                Doctor::get().fatal_if_nullptr(
                    target_symbol,
                    WaspStage::Semantics
                );
                bind_identifier(id, target_symbol);
            },
            [&](MemberAccess& access)
            {
                auto member_symbol = get_module_member_symbol(access);
                target_symbol = member_symbol;
                access.right->as<Identifier>().symbol = target_symbol;
            },
            [&](auto&)
            {
                Doctor::get().fatal(
                    WaspStage::Semantics,
                    "Invalid target expression."
                );
            }
        },
        target->data
    );

    return target_symbol;
}

Object_ptr SemanticAnalyzer::call_concrete_template(
    Call& call,
    TemplateAngular& template_angular,
    const ObjectVector& argument_types
)
{
    ObjectVector concrete_arguments;

    for (const auto& node : template_angular.angular_nodes)
    {
        concrete_arguments.push_back(visit(node));
    }

    Symbol_ptr overload_symbol = resolve_target_symbol(template_angular.target);
    template_angular.symbol = overload_symbol;

    Doctor::get().assert(
        overload_symbol->payload_is<OverloadsData>(),
        WaspStage::Semantics,
        "Symbol '" + overload_symbol->name + "' must hold function overloads."
    );

    auto candidates = overload_symbol->get_payload_as<OverloadsData>()
                          .get_overloads();

    auto generic_candidates = type_system->filter_by_generic_arity(
        candidates,
        concrete_arguments.size()
    );

    Doctor::get().assert(
        !generic_candidates.empty(),
        WaspStage::Semantics,
        "No generic functions with required arity found for '" +
            overload_symbol->name + "'"
    );

    auto
        [specialized_candidates,
         original_indices] = type_system
                                 ->specialize_candidates(
                                     generic_candidates,
                                     concrete_arguments
                                 );

    auto [function_object, subset_index] = type_system
                                               ->get_best_function_object(
                                                   current_scope,
                                                   specialized_candidates,
                                                   argument_types
                                               );

    call.overload_index = original_indices[subset_index];

    return function_object->as<Signature_ptr>()->return_type;
}

// ============================================================================
// Constructors
// ============================================================================

Object_ptr SemanticAnalyzer::visit(Constructor& constructor)
{
    ObjectVector argument_types = visit(constructor.values);

    Object_ptr target_type = nullptr;
    std::visit(
        overloaded{
            [&](Identifier& target)
            {
                target_type = visit(target);
            },
            [&](TemplateAngular& tc)
            {
                target_type = visit(tc);
            },
            [&](auto&)
            {
                Doctor::get().fatal(
                    WaspStage::Semantics,
                    "Invalid constructor target"
                );
            }
        },
        constructor.construtable->data
    );

    Doctor::get().assert(
        target_type && target_type->is<ClassType_ptr>(),
        WaspStage::Semantics,
        "Constructor target must resolve to a class."
    );

    auto class_type = target_type->as<ClassType_ptr>();

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
            "Constructor Argument Type Mismatch for field '" +
                class_type->fields[i] + "'."
        );
    }

    return target_type;
}

} // namespace Wasp
