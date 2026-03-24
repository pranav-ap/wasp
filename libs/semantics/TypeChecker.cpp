#include "TypeChecker.h"
#include "Doctor.h"
#include "Objects.h"

#include "SymbolScope.h"
#include "Token.h"
#include "Workspace.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <string>
#include <variant>
#include <vector>

#define MAKE_OBJECT_VARIANT(x) std::make_shared<Object>(x)

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

// ============================================================================
// Overload Resolution
// ============================================================================

Symbol_ptr TypeChecker::resolve_from_overload_list(
    SymbolScope_ptr scope,
    const SymbolVector& overloads,
    const ObjectVector& arg_types) const
{
    std::vector<Symbol_ptr> exact_matches;
    std::vector<Symbol_ptr> assignable_matches;

    for (const auto& sym : overloads)
    {
        Doctor::get().assert(
            sym->payload_is<FunctionData>(),
            WaspStage::Semantics,
            "Not a function");

        auto func_type_obj = sym->get_payload_as<FunctionData>().type;
        const auto& func_type = func_type_obj->as<FunctionType>();

        // Check Number of arguments
        if (func_type.input_types.size() != arg_types.size())
        {
            continue;
        }

        // Check for an EXACT type match
        if (equal(scope, func_type.input_types, arg_types))
        {
            exact_matches.push_back(sym);
            continue;
        }

        // Check for an ASSIGNABLE type match
        if (assignable(scope, func_type.input_types, arg_types))
        {
            assignable_matches.push_back(sym);
        }
    }

    // Exact matches take highest priority
    if (!exact_matches.empty())
    {
        if (exact_matches.size() > 1)
        {
            Doctor::get().fatal(
                WaspStage::Semantics,
                "Ambiguous function call: multiple exact matches found.");
        }
        return exact_matches[0];
    }

    // Fallback to assignable matches
    if (!assignable_matches.empty())
    {
        if (assignable_matches.size() > 1)
        {
            Doctor::get().fatal(
                WaspStage::Semantics,
                "Ambiguous function call: multiple assignable matches.");
        }
        return assignable_matches[0];
    }

    // If we reach here, nothing matched!
    return nullptr;
}

Symbol_ptr TypeChecker::resolve_function_overload(
    SymbolScope_ptr scope,
    const std::string& func_name,
    const ObjectVector& arg_types) const
{
    auto overloads = scope->get_function_overloads(func_name);

    Symbol_ptr resolved = resolve_from_overload_list(scope, overloads, arg_types);

    Doctor::get().fatal_if_nullptr(
        resolved,
        WaspStage::Semantics,
        "No matching function overload found for standard function '" + func_name + "'");

    return resolved;
}

Symbol_ptr TypeChecker::resolve_module_function_overload(
    SymbolScope_ptr scope,
    const std::string& module_name,
    const std::string& func_name,
    const ObjectVector& arg_types) const
{
    auto overloads = scope->get_function_overloads_from_module(module_name, func_name);

    Symbol_ptr resolved = resolve_from_overload_list(scope, overloads, arg_types);

    Doctor::get().fatal_if_nullptr(
        resolved,
        WaspStage::Semantics,
        "No matching function overload found for '" + module_name + "." + func_name);

    return resolved;
}

void TypeChecker::validate_new_overload(
    SymbolScope_ptr scope,
    const std::string& func_name,
    const ObjectVector& new_param_types) const
{
    auto existing_overloads = scope->get_function_overloads(func_name);

    for (const auto& sym : existing_overloads)
    {
        auto func_type_obj = sym->get_payload_as<FunctionData>().type;

        if (func_type_obj == nullptr)
        {
            continue;
        }

        Doctor::get().assert(
            func_type_obj->is<FunctionType>(),
            WaspStage::Semantics,
            "Overloaded symbol has a non-function type");

        const auto& func_type = func_type_obj->as<FunctionType>();

        // If the number of parameters is different, there is zero risk of collision!
        if (func_type.input_types.size() != new_param_types.size())
        {
            continue;
        }

        // EXACT MATCH AMBIGUITY CHECK
        if (equal(scope, func_type.input_types, new_param_types))
        {
            Doctor::get().fatal(
                WaspStage::Semantics,
                "A function named '" + func_name + "' with this exact signature already exists.");
        }

        // ASSIGNABLE AMBIGUITY CHECK
        if (assignable(scope, func_type.input_types, new_param_types))
        {
            Doctor::get().fatal(
                WaspStage::Semantics,
                "Signature for '" + func_name +
                    "' overlaps ambiguously with an existing overload.");
        }
    }
}

// ============================================================================
// Equal Checks
// ============================================================================

bool TypeChecker::equal(SymbolScope_ptr scope, const Object_ptr type_1, const Object_ptr type_2)
    const
{
    if (!type_1 || !type_2)
        return false;
    if (type_1 == type_2)
        return true;

    // Extract Variant logic outside to prevent template explosion
    if (type_1->is<VariantType>() && type_2->is<VariantType>())
    {
        return equal_unordered(
            scope,
            type_1->as<VariantType>().types,
            type_2->as<VariantType>().types);
    }

    return std::visit(
        overloaded{
            // Standard Types
            [&](AnyType const&, AnyType const&) -> bool { return true; },
            [&](IntType const&, IntType const&) -> bool { return true; },
            [&](FloatType const&, FloatType const&) -> bool { return true; },
            [&](BooleanType const&, BooleanType const&) -> bool { return true; },
            [&](StringType const&, StringType const&) -> bool { return true; },
            [&](NoneType const&, NoneType const&) -> bool { return true; },

            // Literal Types (Must have exact same value)
            [&](IntLiteralType const& t1, IntLiteralType const& t2) -> bool
            { return t1.value == t2.value; },
            [&](FloatLiteralType const& t1, FloatLiteralType const& t2) -> bool
            { return t1.value == t2.value; },
            [&](BooleanLiteralType const& t1, BooleanLiteralType const& t2) -> bool
            { return t1.value == t2.value; },
            [&](StringLiteralType const& t1, StringLiteralType const& t2) -> bool
            { return t1.value == t2.value; },

            // Composites
            [&](ListType const& t1, ListType const& t2) -> bool
            { return equal(scope, t1.element_type, t2.element_type); },
            [&](SetType const& t1, SetType const& t2) -> bool
            { return equal(scope, t1.element_type, t2.element_type); },
            [&](TupleType const& t1, TupleType const& t2) -> bool
            { return equal(scope, t1.element_types, t2.element_types); },
            [&](MapType const& t1, MapType const& t2) -> bool
            {
                return equal(scope, t1.key_type, t2.key_type) &&
                       equal(scope, t1.value_type, t2.value_type);
            },

            [](const auto&, const auto&) -> bool { return false; }},
        type_1->value,
        type_2->value);
}

bool TypeChecker::equal(
    SymbolScope_ptr scope,
    const ObjectVector type_vector_1,
    const ObjectVector type_vector_2) const
{
    if (type_vector_1.size() != type_vector_2.size())
        return false;

    for (size_t i = 0; i < type_vector_1.size(); i++)
    {
        if (!equal(scope, type_vector_1[i], type_vector_2[i]))
            return false;
    }

    return true;
}

bool TypeChecker::equal_unordered(
    SymbolScope_ptr scope,
    const ObjectVector left_vector,
    const ObjectVector right_vector) const
{
    if (left_vector.size() != right_vector.size())
        return false;
    for (auto left : left_vector)
    {
        bool found = std::any_of(
            right_vector.begin(),
            right_vector.end(),
            [&](auto right) { return equal(scope, left, right); });
        if (!found)
            return false;
    }
    return true;
}

// ============================================================================
// Assignable Checks (LHS <- RHS)
// ============================================================================
bool TypeChecker::assignable(
    SymbolScope_ptr scope,
    const Object_ptr lhs_type,
    const Object_ptr rhs_type) const
{
    if (!lhs_type || !rhs_type)
        return false;
    if (equal(scope, lhs_type, rhs_type))
        return true;

    // if RHS is a variant, ALL its elements must fit LHS
    if (rhs_type->is<VariantType>())
    {
        auto& rhs_var = rhs_type->as<VariantType>();
        return std::all_of(
            rhs_var.types.begin(),
            rhs_var.types.end(),
            [&](Object_ptr t) { return assignable(scope, lhs_type, t); });
    }

    // if LHS is a variant, RHS must fit into AT LEAST ONE element of LHS
    if (lhs_type->is<VariantType>())
    {
        auto& lhs_var = lhs_type->as<VariantType>();
        return std::any_of(
            lhs_var.types.begin(),
            lhs_var.types.end(),
            [&](Object_ptr t) { return assignable(scope, t, rhs_type); });
    }

    return std::visit(
        overloaded{
            [](AnyType const&, const auto&) -> bool { return true; },

            [](IntType const&, IntType const&) -> bool { return true; },
            [](FloatType const&, FloatType const&) -> bool { return true; },
            [](BooleanType const&, BooleanType const&) -> bool { return true; },
            [](StringType const&, StringType const&) -> bool { return true; },

            // Standard types accept their literal counterparts
            [](IntType const&, IntLiteralType const&) -> bool { return true; },
            [](FloatType const&, FloatLiteralType const&) -> bool { return true; },
            [](BooleanType const&, BooleanLiteralType const&) -> bool { return true; },
            [](StringType const&, StringLiteralType const&) -> bool { return true; },

            // Handle nested composite types recursively
            [&](ListType const& t1, ListType const& t2) -> bool
            { return assignable(scope, t1.element_type, t2.element_type); },
            [&](SetType const& t1, SetType const& t2) -> bool
            { return assignable(scope, t1.element_type, t2.element_type); },
            [&](TupleType const& t1, TupleType const& t2) -> bool
            { return assignable(scope, t1.element_types, t2.element_types); },
            [&](MapType const& t1, MapType const& t2) -> bool
            {
                return assignable(scope, t1.key_type, t2.key_type) &&
                       assignable(scope, t1.value_type, t2.value_type);
            },

            // Catch-all: Types do not match
            [](const auto&, const auto&) -> bool { return false; }

        },
        lhs_type->value,
        rhs_type->value);
}

bool TypeChecker::assignable(
    SymbolScope_ptr scope,
    const ObjectVector type_vector_1,
    const ObjectVector type_vector_2) const
{
    if (type_vector_1.size() != type_vector_2.size())
        return false;
    for (size_t i = 0; i < type_vector_1.size(); i++)
    {
        if (!assignable(scope, type_vector_1[i], type_vector_2[i]))
            return false;
    }
    return true;
}

// ============================================================================
// Inference
// ============================================================================

Object_ptr TypeChecker::infer(
    SymbolScope_ptr scope,
    Object_ptr left_type,
    TokenType op,
    Object_ptr right_type)
{
    switch (op)
    {
    case TokenType::PLUS:
        // String Concatenation
        if (is_string_type(left_type))
        {
            Doctor::get().assert(
                is_string_type(right_type) && is_number_type(right_type),
                WaspStage::Semantics,
                "Cannot concatenate string with this type");

            return pool->get_string_type();
        }
    case TokenType::STAR:
    case TokenType::POWER:
    case TokenType::MINUS:
    case TokenType::DIVISION:
    case TokenType::REMINDER:
    {
        expect_number_type(left_type);
        expect_number_type(right_type);

        if (is_float_type(left_type) || is_float_type(right_type))
        {
            return pool->get_float_type();
        }
        return pool->get_int_type();
    }
    case TokenType::LESSER_THAN:
    case TokenType::LESSER_THAN_EQUAL:
    case TokenType::GREATER_THAN:
    case TokenType::GREATER_THAN_EQUAL:
    {
        expect_number_type(left_type);
        expect_number_type(right_type);
        return pool->get_boolean_type();
    }
    case TokenType::EQUAL_EQUAL:
    case TokenType::BANG_EQUAL:
    {
        if (is_number_type(left_type))
        {
            expect_number_type(right_type);
        }
        else if (is_string_type(left_type))
        {
            expect_string_type(right_type);
        }
        else if (is_boolean_type(left_type))
        {
            expect_boolean_type(right_type);
        }
        else
        {
            Doctor::get().fatal(WaspStage::Semantics, "Unsupported types for equality comparison");
        }

        return pool->get_boolean_type();
    }
    case TokenType::AND:
    case TokenType::OR:
    {
        expect_boolean_type(left_type);
        expect_boolean_type(right_type);
        return pool->get_boolean_type();
    }
    default:
    {
        Doctor::get().fatal(WaspStage::Semantics, "Unknown binary operator");
    }
    }
    return pool->get_none_type();
}

Object_ptr TypeChecker::infer(SymbolScope_ptr scope, Object_ptr operand_type, TokenType op)
{
    switch (op)
    {
    case TokenType::PLUS:
    case TokenType::MINUS:
    {

        expect_number_type(operand_type);
        return is_int_type(operand_type) ? pool->get_int_type() : pool->get_float_type();
    }
    case TokenType::NOT:
    {

        expect_boolean_type(operand_type);
        return pool->get_boolean_type();
    }
    default:
    {
        Doctor::get().fatal(WaspStage::Semantics, "Unknown unary operator");
    }
    }
    return pool->get_none_type();
}

// ============================================================================
// Utils
// ============================================================================

Object_ptr TypeChecker::spread_type(Object_ptr type)
{
    if (!type)
        return nullptr;

    return std::visit(
        overloaded{
            [&](ListType const& t) { return t.element_type; },
            [&](TupleType const& t) { return MAKE_OBJECT_VARIANT(VariantType(t.element_types)); },
            [&](MapType const& t)
            {
                // Spreading a map returns elements that are Tuples of (Key, Value)
                ObjectVector kv_types = {t.key_type, t.value_type};
                return MAKE_OBJECT_VARIANT(TupleType(kv_types));
            },
            [&](const auto&)
            {
                Doctor::get().fatal(WaspStage::Semantics, "Cannot spread a non-iterable type");
                return pool->get_none_type();
            }},
        type->value);
}

Object_ptr TypeChecker::extract_iterable_element(SymbolScope_ptr scope, const Object_ptr type) const
{
    if (!type)
    {
        return MAKE_OBJECT_VARIANT(AnyType());
    }

    if (type->is<VariantType>())
    {
        auto& variant = type->as<VariantType>();
        ObjectVector extracted_elements;

        for (const auto& t : variant.types)
        {
            extracted_elements.push_back(extract_iterable_element(scope, t));
        }

        ObjectVector unique_elements = remove_duplicates(scope, extracted_elements);

        if (unique_elements.size() == 1)
        {
            return unique_elements[0];
        }

        return MAKE_OBJECT_VARIANT(VariantType(unique_elements));
    }

    return std::visit(
        overloaded{
            [](ListType const& t) -> Object_ptr { return t.element_type; },

            [](SetType const& t) -> Object_ptr { return t.element_type; },

            [](MapType const& t) -> Object_ptr
            { return MAKE_OBJECT_VARIANT(TupleType({t.key_type, t.value_type})); },

            [&](TupleType const& t) -> Object_ptr
            {
                // Iterating over a Tuple means the loop variable could be ANY of its elements
                ObjectVector unique_elements = remove_duplicates(scope, t.element_types);
                if (unique_elements.size() == 1)
                    return unique_elements[0];
                return MAKE_OBJECT_VARIANT(VariantType(unique_elements));
            },

            [&](StringType const&) -> Object_ptr { return pool->get_string_type(); },

            [&](const auto&) -> Object_ptr { return pool->get_any_type(); }},
        type->value);
}

// ============================================================================
// Type Checks
// ============================================================================

bool TypeChecker::is_boolean_type(const Object_ptr type) const
{
    return type && (holds_alternative<BooleanType>(type->value) ||
                    holds_alternative<BooleanLiteralType>(type->value));
}

bool TypeChecker::is_int_type(const Object_ptr type) const
{
    return type && (holds_alternative<IntType>(type->value) ||
                    holds_alternative<IntLiteralType>(type->value));
}

bool TypeChecker::is_float_type(const Object_ptr type) const
{
    return type && (holds_alternative<FloatType>(type->value) ||
                    holds_alternative<FloatLiteralType>(type->value));
}

bool TypeChecker::is_string_type(const Object_ptr type) const
{
    return type && (holds_alternative<StringType>(type->value) ||
                    holds_alternative<StringLiteralType>(type->value));
}

bool TypeChecker::is_none_type(const Object_ptr type) const
{
    return type && holds_alternative<NoneType>(type->value);
}

bool TypeChecker::is_number_type(const Object_ptr type) const
{
    return is_int_type(type) || is_float_type(type);
}

bool TypeChecker::is_condition_type(SymbolScope_ptr scope, const Object_ptr condition_type) const
{
    if (!condition_type)
        return false;

    return std::visit(
        overloaded{
            [](BooleanType const&) { return true; },
            [](BooleanLiteralType const&) { return true; },

            // Truthiness evaluated at runtime based on length
            [](StringType const&) { return true; },
            [](StringLiteralType const&) { return true; },

            // Truthiness evaluated at runtime based on size
            [](ListType const&) { return true; },
            [](TupleType const&) { return true; },
            [](SetType const&) { return true; },
            [](MapType const&) { return true; },

            // If it's a Variant (e.g., `string | bool`), all possible types must be valid
            // conditions
            [&](VariantType const& type)
            {
                return std::all_of(
                    type.types.begin(),
                    type.types.end(),
                    [&](Object_ptr t) { return is_condition_type(scope, t); });
            },

            // Anything else (Functions, NoneType, etc.) is rejected
            [](const auto&) { return false; }},
        condition_type->value);
}

bool TypeChecker::is_spreadable_type(SymbolScope_ptr scope, const Object_ptr candidate_type) const
{
    if (!candidate_type)
        return false;
    return std::visit(
        overloaded{
            [&](ListType const&) { return true; },
            [&](TupleType const&) { return true; },
            [&](MapType const&) { return true; },
            [&](VariantType const& type)
            {
                return std::all_of(
                    type.types.begin(),
                    type.types.end(),
                    [&](Object_ptr t) { return is_spreadable_type(scope, t); });
            },
            [](const auto&) { return false; }},
        candidate_type->value);
}

bool TypeChecker::is_iterable_type(SymbolScope_ptr scope, const Object_ptr candidate_type) const
{
    return is_string_type(candidate_type) || is_spreadable_type(scope, candidate_type);
}

bool TypeChecker::is_key_type(SymbolScope_ptr scope, const Object_ptr key_type) const
{
    return is_int_type(key_type) || is_float_type(key_type) || is_string_type(key_type) ||
           is_boolean_type(key_type);
}

// ============================================================================
// Assert Type
// ============================================================================

void TypeChecker::expect_boolean_type(const Object_ptr type) const
{
    Doctor::get().assert(is_boolean_type(type), WaspStage::Semantics, "Expected a boolean type");
}

void TypeChecker::expect_number_type(const Object_ptr type) const
{
    Doctor::get().assert(is_number_type(type), WaspStage::Semantics, "Expected a number type");
}

void TypeChecker::expect_int_type(const Object_ptr type) const
{
    Doctor::get().assert(is_int_type(type), WaspStage::Semantics, "Expected a integer type");
}

void TypeChecker::expect_float_type(const Object_ptr type) const
{
    Doctor::get().assert(is_float_type(type), WaspStage::Semantics, "Expected a float type");
}

void TypeChecker::expect_string_type(const Object_ptr type) const
{
    Doctor::get().assert(is_string_type(type), WaspStage::Semantics, "Expected a string type");
}

void TypeChecker::expect_none_type(const Object_ptr type) const
{
    Doctor::get().assert(is_none_type(type), WaspStage::Semantics, "Expected a None type");
}

void TypeChecker::expect_condition_type(SymbolScope_ptr scope, const Object_ptr type) const
{
    Doctor::get().assert(
        is_condition_type(scope, type),
        WaspStage::Semantics,
        "Expected a valid condition type (boolean)");
}

void TypeChecker::expect_spreadable_type(SymbolScope_ptr scope, const Object_ptr type) const
{
    Doctor::get().assert(
        is_spreadable_type(scope, type),
        WaspStage::Semantics,
        "Expected a spreadable collection type");
}

void TypeChecker::expect_iterable_type(SymbolScope_ptr scope, const Object_ptr type) const
{
    Doctor::get().assert(
        is_iterable_type(scope, type),
        WaspStage::Semantics,
        "Expected an iterable type");
}

void TypeChecker::expect_key_type(SymbolScope_ptr scope, const Object_ptr type) const
{
    Doctor::get().assert(
        is_key_type(scope, type),
        WaspStage::Semantics,
        "Type cannot be used as a Dictionary/Map key");
}

// ============================================================================
// Type Utilities
// ============================================================================

bool TypeChecker::any_eq(SymbolScope_ptr scope, const ObjectVector vec, const Object_ptr x) const
{
    for (const auto& item : vec)
    {
        if (equal(scope, item, x))
        {
            return true;
        }
    }
    return false;
}

ObjectVector TypeChecker::remove_duplicates(SymbolScope_ptr scope, const ObjectVector vec) const
{
    ObjectVector unique_types;
    unique_types.reserve(vec.size());

    for (const auto& item : vec)
    {
        if (!any_eq(scope, unique_types, item))
        {
            unique_types.push_back(item);
        }
    }

    return unique_types;
}
} // namespace Wasp
