#include "AST.h"
#include "Doctor.h"
#include "Expression.h"
#include "Objects.h"
#include "Resolvable.h"
#include "SemanticAnalyzer.h"
#include "Statement.h"
#include "SymbolScope.h"
#include "Token.h"
#include "Workspace.h"

#include <cctype>
#include <ctime>
#include <memory>
#include <string>
#include <vector>

namespace Wasp
{

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

Symbol_ptr SemanticAnalyzer::get_core_symbol(const std::string& type_name)
{
    Symbol_ptr symbol = current_scope->lookup(type_name);
    Doctor::get().fatal_if_nullptr(
        symbol,
        WaspStage::Semantics,
        "Type '" + type_name + "' not found in scope."
    );

    return symbol;
}

Object_ptr SemanticAnalyzer::resolve_literal(
    Resolvable& expr,
    const std::string& type_name,
    Object_ptr type_obj
)
{
    expr.symbol = get_core_symbol(type_name);
    return type_obj;
}

Object_ptr SemanticAnalyzer::visit(IntegerLiteral& expr)
{
    return resolve_literal(expr, "int", workspace->pool->get_int_type());
}

Object_ptr SemanticAnalyzer::visit(FloatLiteral& expr)
{
    return resolve_literal(expr, "float", workspace->pool->get_float_type());
}

Object_ptr SemanticAnalyzer::visit(StringLiteral& expr)
{
    return resolve_literal(expr, "str", workspace->pool->get_string_type());
}

Object_ptr SemanticAnalyzer::visit(BooleanLiteral& expr)
{
    return resolve_literal(expr, "bool", workspace->pool->get_boolean_type());
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
    return nullptr;
}

Object_ptr SemanticAnalyzer::resolve_operator_overload(
    OperatorExpression& expr,
    const std::string& operator_name,
    const ObjectVector& operand_types
)
{
    Symbol_ptr operator_symbol = current_scope->lookup(operator_name);

    Doctor::get().fatal_if_nullptr(
        operator_symbol,
        WaspStage::Semantics,
        "Undefined operator '" + operator_name + "'."
    );

    operator_symbol = operator_symbol->resolve();

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
                                                     operand_types
                                                 );

    expr.symbol = operator_symbol;
    expr.overload_index = overload_index;

    return function_symbol->get_type()->as<Signature_ptr>()->return_type;
}

Object_ptr SemanticAnalyzer::evaluate_operator(
    OperatorExpression& expr,
    TokenType fixity,
    TokenType op_type,
    const ObjectVector& operand_types
)
{
    // 1. Check for OOP Overloads
    for (const auto& type : operand_types)
    {
        if (type->is_any_of<ClassType_ptr, TraitType_ptr>())
        {
            return resolve_operator_overload(
                expr,
                get_operator_name(fixity, op_type),
                operand_types
            );
        }
    }

    // 2. Standard Inference
    if (operand_types.size() == 1)
    {
        return type_system->infer(current_scope, operand_types[0], op_type);
    }

    return type_system
        ->infer(current_scope, operand_types[0], op_type, operand_types[1]);
}

Object_ptr SemanticAnalyzer::visit(Prefix& expr)
{
    return evaluate_operator(
        expr,
        TokenType::PREFIX,
        expr.op.type,
        {visit(expr.operand)}
    );
}

Object_ptr SemanticAnalyzer::visit(Infix& expr)
{
    return evaluate_operator(
        expr,
        TokenType::INFIX,
        expr.op.type,
        {visit(expr.left), visit(expr.right)}
    );
}

Object_ptr SemanticAnalyzer::visit(Postfix& expr)
{
    return evaluate_operator(
        expr,
        TokenType::POSTFIX,
        expr.op.type,
        {visit(expr.operand)}
    );
}

// ============================================================================
// Collections
// ============================================================================

Object_ptr SemanticAnalyzer::collapse_types(const ObjectVector& types)
{
    if (types.empty())
    {
        return make_object(AnyType());
    }

    ObjectVector unique_types = type_system->remove_duplicates(
        current_scope,
        types
    );

    if (unique_types.size() == 1)
    {
        return unique_types[0];
    }

    return make_object(VariantType(unique_types));
}

Object_ptr SemanticAnalyzer::visit(ListLiteral& expr)
{
    return make_object(ListType(collapse_types(visit(expr.expressions))));
}

Object_ptr SemanticAnalyzer::visit(TupleLiteral& expr)
{
    return make_object(TupleType(visit(expr.expressions)));
}

Object_ptr SemanticAnalyzer::visit(SetLiteral& expr)
{
    ObjectVector types = visit(expr.expressions);
    for (auto& type : types)
    {
        type_system->expect_key_type(current_scope, type);
    }
    return make_object(SetType(collapse_types(types)));
}

Object_ptr SemanticAnalyzer::visit(MapLiteral& expr)
{
    ObjectVector key_types, val_types;
    for (const auto& [k_expr, v_expr] : expr.pairs)
    {
        Object_ptr k_type = visit(k_expr);
        type_system->expect_key_type(current_scope, k_type);
        key_types.push_back(k_type);
        val_types.push_back(visit(v_expr));
    }

    return make_object(
        MapType(collapse_types(key_types), collapse_types(val_types))
    );
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

// ============================================================================
// Resolvers
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

    return member_symbol;
}

Symbol_ptr SemanticAnalyzer::resolve_target_symbol(Expression_ptr target)
{
    Symbol_ptr target_symbol = nullptr;

    if (auto* id = target->try_as<Identifier>())
    {
        Symbol_ptr unresolved = current_scope->lookup(id->name);
        Doctor::get().fatal_if_nullptr(
            unresolved,
            WaspStage::Semantics,
            "Undefined identifier: '" + id->name + "'"
        );

        target_symbol = unresolved->resolve();
        bind_identifier(*id, target_symbol);
    }
    else if (auto* access = target->try_as<MemberAccess>())
    {
        auto member_symbol = get_module_member_symbol(*access);
        target_symbol = member_symbol;
        access->right->as<Identifier>().symbol = target_symbol;
    }
    else
    {
        Doctor::get().fatal(WaspStage::Semantics, "Invalid target expression.");
    }

    return target_symbol;
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

    if (auto type = try_unwrap_ptr<ModuleType_ptr>(left_type))
    {
        expr.member_index = type->get_member_index(member_name);
        return type->get_member(member_name);
    }
    else if (auto type = try_unwrap_ptr<ClassType_ptr>(left_type))
    {
        expr.member_index = type->get_member_index(member_name);
        return type->get_member(member_name);
    }
    else if (auto type = try_unwrap_ptr<EnumType_ptr>(left_type))
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
    }

    Doctor::get().fatal(
        WaspStage::Semantics,
        "Cannot access member '" + member_name +
            "'. LHS is not a module, class, trait, or enum."
    );
    return nullptr;
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
    return nullptr;
}

// ============================================================================
// Calls
// ============================================================================

Object_ptr SemanticAnalyzer::visit(Call& call)
{
    ObjectVector argument_types = visit(call.arguments);

    if (auto* identifier = call.callable->try_as<Identifier>())
    {
        auto unresolved = current_scope->lookup(identifier->name);
        Doctor::get().fatal_if_nullptr(
            unresolved,
            WaspStage::Semantics,
            "Undefined callable: '" + identifier->name + "'"
        );

        auto symbol = unresolved->resolve();
        bind_identifier(*identifier, symbol);

        return resolve_standard_overload(call, symbol, argument_types);
    }
    else if (auto* concrete_template = call.callable->try_as<TemplateAngular>())
    {
        return call_concrete_template(call, *concrete_template, argument_types);
    }
    else if (auto* access = call.callable->try_as<MemberAccess>())
    {
        Object_ptr left_type = visit(access->left);

        if (auto class_type = try_unwrap_ptr<ClassType_ptr>(left_type))
        {
            return call_method(call, *access, argument_types, class_type);
        }
        else if (auto module_type = try_unwrap_ptr<ModuleType_ptr>(left_type))
        {
            auto member_symbol = get_module_member_symbol(*access);
            access->right->as<Identifier>().symbol = member_symbol;

            return resolve_standard_overload(
                call,
                member_symbol,
                argument_types
            );
        }

        Doctor::get().fatal(
            WaspStage::Semantics,
            "LHS of call must be a module or class type."
        );
    }

    Doctor::get().fatal(
        WaspStage::Semantics,
        "Expected an Identifier or MemberAccess as the callable."
    );
    return nullptr;
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

    auto candidates = overload_symbol->get_payload_as<OverloadsData>()
                          .get_overloads();

    auto [function_symbol, overload_index] = type_system
                                                 ->get_best_function_symbol(
                                                     current_scope,
                                                     candidates,
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

    return signature_obj->as<Signature_ptr>()->return_type;
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

    if (auto* target = constructor.construtable->try_as<Identifier>())
    {
        target_type = visit(*target);
    }
    else if (auto* tc = constructor.construtable->try_as<TemplateAngular>())
    {
        target_type = visit(*tc);
    }
    else
    {
        Doctor::get().fatal(WaspStage::Semantics, "Invalid constructor target");
    }

    target_type = unwrap_type_alias(target_type);

    auto class_type = try_unwrap_ptr<ClassType_ptr>(target_type);
    Doctor::get().assert(
        class_type != nullptr,
        WaspStage::Semantics,
        "Constructor target must resolve to a class."
    );

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
