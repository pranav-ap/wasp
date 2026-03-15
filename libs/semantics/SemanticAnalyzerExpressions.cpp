#include "Doctor.h"
#include "Expression.h"
#include "Objects.h"
#include "SemanticAnalyzer.h"
#include "Statement.h"
#include "Symbol.h"
#include "SymbolScope.h"
#include "Token.h"

#include <cstddef>
#include <ctime>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <variant>

#define MAKE_OBJECT_VARIANT(x) std::make_shared<Object>(x)

template <class... Ts> struct overloaded : Ts... {
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp {
void SemanticAnalyzer::visit(ExpressionStatement& statement) { visit(statement.expression); }

ObjectVector SemanticAnalyzer::visit(ExpressionVector expressions) {
    ObjectVector computed_types;
    computed_types.reserve(expressions.size());

    for (const auto& expr : expressions) {
        computed_types.push_back(visit(expr));
    }

    return computed_types;
}

Object_ptr SemanticAnalyzer::visit(const Expression_ptr expr) {
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
            [](auto&) -> Object_ptr {
                Doctor::get().Doctor::get().fatal(
                    WaspStage::Semantics, "Unhandled Expression in Semantic Analyzer!"
                );
            }
        },
        expr->data
    );
}

Object_ptr SemanticAnalyzer::visit(Identifier& expr) {
    Symbol_ptr target_symbol = current_scope->lookup(expr.name);
    Doctor::get().fatal_if_nullptr(target_symbol, WaspStage::Semantics);

    // Only variables need to be captured by closures
    if (target_symbol->is<VariableData>()) {
        auto& var_data = target_symbol->as<VariableData>();
        target_symbol->capture_if_required(current_scope->get_closure_depth());
    }

    expr.symbol = target_symbol;

    return target_symbol->get_type();
}

Object_ptr SemanticAnalyzer::access_member(
    const ModuleType& left_type, const Identifier& right_identifier
) {
    std::string member_name = right_identifier.name;
    auto it = left_type.members.find(member_name);

    Doctor::get().assert(
        it != left_type.members.end(),
        WaspStage::Semantics,
        "Namespace has no member named '" + member_name + "'."
    );

    return it->second;
}

Object_ptr SemanticAnalyzer::access_member(const ModuleType& left_type, const Call& right_call) {
    Doctor::get().assert(
        right_call.callable->is<Identifier>(),
        WaspStage::Semantics,
        "Expected RHS of '.' to be an identifier when accessing a namespace."
    );

    auto& right_idenitifer = right_call.callable->as<Identifier>();
    return access_member(left_type, right_idenitifer);
}

Object_ptr SemanticAnalyzer::access_member(const ModuleType& left_type, Expression_ptr right_expr) {
    return std::visit(
        overloaded{
            [&](Identifier& id) { return access_member(left_type, id); },
            [&](Call& call) { return access_member(left_type, call); },
            [](auto&) -> Object_ptr {
                Doctor::get().fatal(
                    WaspStage::Semantics,
                    "RHS of member access must be either an identifier or a call."
                );
                return nullptr;
            }
        },
        right_expr->data
    );
}

Object_ptr SemanticAnalyzer::visit(MemberAccess& expr) {
    Object_ptr left_type = visit(expr.left);

    Doctor::get().assert(
        left_type->is<ModuleType>(),
        WaspStage::Semantics,
        "Incorrect type for LHS of member access."
    );

    return access_member(left_type->as<ModuleType>(), expr.right);
}

Object_ptr SemanticAnalyzer::visit(Call& call_expr) {
    Object_ptr callee_type = visit(call_expr.callable);

    Doctor::get().assert(
        callee_type->is<FunctionType>(),
        WaspStage::Semantics,
        "Expression is not callable. Type is: " + Wasp::stringify_object(callee_type)
    );

    if (call_expr.callable->is<Identifier>()) {
        call_expr.symbol = call_expr.callable->as<Identifier>().symbol;
    }

    const auto& function_signature = callee_type->as<FunctionType>();

    // 2. Check Argument Counts
    Doctor::get().assert(
        call_expr.arguments.size() == function_signature.input_types.size(),
        WaspStage::Semantics,
        "Incorrect number of arguments. Expected " +
            std::to_string(function_signature.input_types.size()) + ", got " +
            std::to_string(call_expr.arguments.size()) + "."
    );

    // 3. Type-Check Each Argument
    for (size_t i = 0; i < call_expr.arguments.size(); ++i) {
        Object_ptr argument_type = visit(call_expr.arguments[i]);
        Object_ptr parameter_type = function_signature.input_types[i];

        Doctor::get().assert(
            type_checker->assignable(current_scope, parameter_type, argument_type),
            WaspStage::Semantics,
            "Argument mismatch at index " + std::to_string(i) + ". Expected " +
                Wasp::stringify_object(parameter_type) + ", got " +
                Wasp::stringify_object(argument_type) + "."
        );
    }

    if (function_signature.return_type.has_value()) {
        return function_signature.return_type.value();
    }

    return MAKE_OBJECT_VARIANT(NoneType());
}

Object_ptr SemanticAnalyzer::visit(int expr) { return MAKE_OBJECT_VARIANT(IntType()); }

Object_ptr SemanticAnalyzer::visit(double expr) { return MAKE_OBJECT_VARIANT(FloatType()); }

Object_ptr SemanticAnalyzer::visit(std::string expr) { return MAKE_OBJECT_VARIANT(StringType()); }

Object_ptr SemanticAnalyzer::visit(bool expr) { return MAKE_OBJECT_VARIANT(BooleanType()); }

Object_ptr SemanticAnalyzer::visit(DotLiteral& expr) { return nullptr; }

Object_ptr SemanticAnalyzer::visit(Prefix& expr) {
    Object_ptr right_type = visit(expr.operand);
    return type_checker->infer(current_scope, right_type, expr.op.type);
}

Object_ptr SemanticAnalyzer::visit(Infix& expr) {
    Object_ptr left_type = visit(expr.left);
    Object_ptr right_type = visit(expr.right);
    return type_checker->infer(current_scope, left_type, expr.op.type, right_type);
}

Object_ptr SemanticAnalyzer::visit(Postfix& expr) {
    Object_ptr left_type = visit(expr.operand);
    type_checker->expect_number_type(left_type);
    return left_type;
}

Object_ptr SemanticAnalyzer::visit(ListLiteral& expr) {
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

Object_ptr SemanticAnalyzer::visit(TupleLiteral& expr) {
    ObjectVector element_types;
    for (const auto& element : expr.expressions)
        element_types.push_back(visit(element));
    return MAKE_OBJECT_VARIANT(TupleType(element_types));
}

Object_ptr SemanticAnalyzer::visit(MapLiteral& expr) {
    if (expr.pairs.empty())
        return MAKE_OBJECT_VARIANT(
            MapType(MAKE_OBJECT_VARIANT(AnyType()), MAKE_OBJECT_VARIANT(AnyType()))
        );

    ObjectVector key_types;
    ObjectVector val_types;

    for (const auto& [key_expr, val_expr] : expr.pairs) {
        Object_ptr k_type = visit(key_expr);
        type_checker->expect_key_type(current_scope, k_type);
        key_types.push_back(k_type);
        val_types.push_back(visit(val_expr));
    }

    ObjectVector unique_keys = type_checker->remove_duplicates(current_scope, key_types);
    ObjectVector unique_vals = type_checker->remove_duplicates(current_scope, val_types);

    Object_ptr final_key_type =
        unique_keys.size() == 1 ? unique_keys[0] : MAKE_OBJECT_VARIANT(VariantType(unique_keys));
    Object_ptr final_val_type =
        unique_vals.size() == 1 ? unique_vals[0] : MAKE_OBJECT_VARIANT(VariantType(unique_vals));

    return MAKE_OBJECT_VARIANT(MapType(final_key_type, final_val_type));
}

Object_ptr SemanticAnalyzer::visit(SetLiteral& expr) {
    if (expr.expressions.empty())
        return MAKE_OBJECT_VARIANT(SetType(MAKE_OBJECT_VARIANT(AnyType())));

    ObjectVector element_types;
    for (const auto& element : expr.expressions) {
        Object_ptr el_type = visit(element);
        type_checker->expect_key_type(current_scope, el_type);
        element_types.push_back(el_type);
    }

    ObjectVector unique_types = type_checker->remove_duplicates(current_scope, element_types);

    if (unique_types.size() == 1)
        return MAKE_OBJECT_VARIANT(SetType(unique_types[0]));
    return MAKE_OBJECT_VARIANT(SetType(MAKE_OBJECT_VARIANT(VariantType(unique_types))));
}

Object_ptr SemanticAnalyzer::visit(RangeLiteral& expr) {
    Object_ptr start_type = nullptr;
    Object_ptr end_type = nullptr;

    if (expr.start) {
        start_type = visit(expr.start);
        type_checker->expect_number_type(start_type);
    }

    if (expr.end) {
        end_type = visit(expr.end);
        type_checker->expect_number_type(end_type);
    }

    if (type_checker->is_float_type(start_type) || type_checker->is_float_type(end_type)) {
        return MAKE_OBJECT_VARIANT(ListType(MAKE_OBJECT_VARIANT(FloatType())));
    }

    return MAKE_OBJECT_VARIANT(ListType(MAKE_OBJECT_VARIANT(IntType())));
}

Object_ptr SemanticAnalyzer::visit(TypePattern& expr) { return nullptr; }
} // namespace Wasp
