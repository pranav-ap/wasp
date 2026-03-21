#include "AST.h"
#include "Doctor.h"
#include "Expression.h"
#include "Objects.h"
#include "SemanticAnalyzer.h"
#include "Statement.h"
#include "Symbol.h"
#include "SymbolScope.h"
#include "Token.h"

#include <ctime>
#include <map>
#include <memory>
#include <string>
#include <variant>

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

Object_ptr SemanticAnalyzer::visit(Identifier& expr)
{
    Symbol_ptr target_symbol = current_scope->lookup(expr.name);
    Doctor::get().fatal_if_nullptr(target_symbol, WaspStage::Semantics);

    expr.symbol = target_symbol;

    if (target_symbol->should_be_captured(current_scope->get_closure_depth()))
    {
        expr.must_be_captured = true;
    }

    return target_symbol->get_type();
}

Object_ptr SemanticAnalyzer::visit(Call& call_expr)
{
    ObjectVector arg_types;

    for (auto& arg : call_expr.arguments)
    {
        arg_types.push_back(visit(arg));
    }

    Symbol_ptr resolved_function = type_checker->resolve_function_overload(
        current_scope,
        call_expr.callable.name,
        arg_types);

    Doctor::get().fatal_if_nullptr(
        resolved_function,
        WaspStage::Semantics,
        "No matching function overload found for '" + call_expr.callable.name);

    if (resolved_function->should_be_captured(current_scope->get_closure_depth()))
    {
        call_expr.callable.must_be_captured = true;
    }

    call_expr.callable.symbol = resolved_function;

    auto& final_signature = resolved_function->get_payload_as<FunctionData>()
                                .type->as<FunctionType>();

    if (final_signature.return_type.has_value())
    {
        return final_signature.return_type.value();
    }

    return MAKE_OBJECT_VARIANT(NoneType());
}

Object_ptr SemanticAnalyzer::visit(Call& call_expr)
{
    auto& mac = call_expr.callable->as<MemberAccess>();

    // Let your existing visit(MemberAccess) logic extract the member's type
    Object_ptr member_type = visit(mac);

    Doctor::get().assert(
        member_type->is<FunctionType>(),
        WaspStage::Semantics,
        "Member is not a callable function.");

    const auto& final_signature = member_type->as<FunctionType>();

    // Check that the arguments match the member function's signature
    // Note: You can upgrade this to use TypeChecker::assignable later!
    Doctor::get().assert(
        final_signature.input_types.size() == arg_types.size(),
        WaspStage::Semantics,
        "Argument count mismatch in member function call.");
}

Object_ptr SemanticAnalyzer::get_member_type(
    const ModuleType& module_type,
    const Identifier& identifier) const
{
    Doctor::get().assert(
        module_type.contains_member(identifier.name),
        WaspStage::Semantics,
        "Module has no member named '" + identifier.name);

    return module_type.get_member_type(identifier.name);
}

Object_ptr SemanticAnalyzer::get_member_type(const ModuleType& module_type, const Call& call) const
{
    return std::visit(
        overloaded{
            [&](Identifier& identifier) -> Object_ptr
            { return get_member_type(module_type, identifier); },

            [&](MemberAccess& member_access) -> Object_ptr
            { return get_member_type(module_type, member_access); },

            [&](Call& inner_call) -> Object_ptr
            {
                auto links = inner_call.callable->as<MemberAccess>().flatten_links();

                Doctor::get().assert(
                    !links.empty(),
                    WaspStage::Semantics,
                    "Invalid member access path in call. Expected at least one identifier.");

                return get_member_type(module_type, links);
            },

            [](auto&) -> Object_ptr
            {
                Doctor::get().fatal(
                    WaspStage::Semantics,
                    "Expected an identifier or call in member access chain");
                return nullptr;
            }},
        call.callable->data);
}

Object_ptr SemanticAnalyzer::get_member_type(
    const ModuleType& module_type,
    const Expression_ptr link) const
{
    return std::visit(
        overloaded{
            [&](Identifier& identifier) -> Object_ptr
            { return get_member_type(module_type, identifier); },

            [&](Call& call) -> Object_ptr { return get_member_type(module_type, call); },

            [](auto&) -> Object_ptr
            {
                Doctor::get().fatal(
                    WaspStage::Semantics,
                    "Expected an identifier or call in member access chain");
                return nullptr;
            }},
        link->data);
}

Object_ptr SemanticAnalyzer::get_member_type(
    const ModuleType& module_type,
    const ExpressionVector& chain) const
{
    Doctor::get().assert(
        !chain.empty(),
        WaspStage::Semantics,
        "Chain must have at least one element to resolve member type");

    Object_ptr deepest_type = nullptr;

    for (const auto expr : chain)
    {
        deepest_type = get_member_type(module_type, expr);
    }
}

Object_ptr SemanticAnalyzer::visit(MemberAccess& chain)
{
    auto links = chain.flatten_links();

    Doctor::get().assert(
        !links.empty(),
        WaspStage::Semantics,
        "Invalid member access path. Expected at least one identifier.");

    auto first_link = links[0];
    auto first_link_type = visit(first_link);

    auto other_links = ExpressionVector(links.begin() + 1, links.end());

    return std::visit(
        overloaded{
            [&](ModuleType& module_type) -> Object_ptr
            { return get_member_type(module_type, other_links); },

            [](auto&) -> Object_ptr
            {
                Doctor::get().fatal(
                    WaspStage::Semantics,
                    "Only module namespaces can be accessed with member access.");
                return nullptr;
            }},
        first_link_type->value);
}

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
} // namespace Wasp
