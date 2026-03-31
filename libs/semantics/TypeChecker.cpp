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
#include <tuple>
#include <utility>
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

SymbolVector::iterator TypeChecker::find_matching_signature(
    SymbolScope_ptr scope,
    SymbolVector& target_vector,
    const ObjectVector& parameter_types
)
{
    return std::find_if(
        target_vector.begin(),
        target_vector.end(),
        [&](const Symbol_ptr& sym)
        {
            auto type_obj = sym->get_type();

            // Skip unresolved functions (e.g., hoisted functions we haven't visited yet)
            if (!type_obj || !type_obj->is<FunctionType>())
                return false;

            const auto& existing_signature = type_obj->as<FunctionType>();

            if (existing_signature.input_types.size() != parameter_types.size())
                return false;

            for (size_t i = 0; i < parameter_types.size(); ++i)
            {
                if (!equal(scope, existing_signature.input_types[i], parameter_types[i]))
                    return false;
            }

            return true;
        }
    );
}

void TypeChecker::validate_overload_group(
    SymbolScope_ptr scope,
    std::string& function_name,
    const Symbol_ptr new_function_symbol
)
{
    auto overload_group_symbol = scope->lookup(function_name);

    Doctor::get().assert(
        overload_group_symbol->payload_is<OverloadGroupData>(),
        WaspStage::Semantics,
        "Symbol is not an overload group"
    );

    const auto& parameter_types = new_function_symbol->get_type()->as<FunctionType>().input_types;

    auto& group_data = overload_group_symbol->get_payload_as<OverloadGroupData>();

    // Sibling Duplicate Check

    for (const auto& sibling : group_data.siblings)
    {
        if (sibling == new_function_symbol)
        {
            continue;
        }

        auto type_obj = sibling->get_type();

        // If the sibling hasn't been type-checked yet (only hoisted so far),
        // skip it. Semantic Analyzer will visit it later to check for duplciate
        // siblings
        if (!type_obj)
        {
            continue;
        }

        Doctor::get().assert(
            type_obj->is<FunctionType>(),
            WaspStage::Semantics,
            "Internal Compiler Error: Overload group contains a non-function "
            "symbol"
        );

        const auto& existing_signatures = type_obj->as<FunctionType>();

        if (equal(scope, existing_signatures.input_types, parameter_types))
        {
            Doctor::get().fatal(
                WaspStage::Semantics,
                "Duplicate function signatures for '" + function_name + "' defined in same scope"
            );
        }
    }

    // Shadow Parents
    auto parent_match = find_matching_signature(scope, group_data.parents, parameter_types);

    if (parent_match != group_data.parents.end())
    {
        group_data.parents.erase(parent_match);
    }
}

void TypeChecker::collect_assignable_signatures(
    SymbolScope_ptr scope,
    const SymbolVector& candidates,
    const ObjectVector& argument_types,
    SymbolVector& valid_matches
) const
{
    for (const auto& candidate : candidates)
    {
        auto type_obj = candidate->get_type();

        // Skip unresolved symbols just to be safe
        if (!type_obj || !type_obj->is<FunctionType>())
            continue;

        const auto& signature = type_obj->as<FunctionType>();

        if (assignable(scope, signature.input_types, argument_types))
        {
            valid_matches.push_back(candidate);
        }
    }
}

std::pair<Symbol_ptr, int> TypeChecker::resolve_function_call(
    SymbolScope_ptr scope,
    std::string& function_name,
    const ObjectVector& argument_types
) const
{
    auto overload_group_symbol = scope->lookup(function_name);

    Doctor::get().assert(
        overload_group_symbol->payload_is<OverloadGroupData>(),
        WaspStage::Semantics,
        "Symbol is not an overload group"
    );

    const auto& group_data = overload_group_symbol->get_payload_as<OverloadGroupData>();

    SymbolVector valid_matches;

    collect_assignable_signatures(
        scope,
        group_data.get_all_overloads(),
        argument_types,
        valid_matches
    );

    Doctor::get().assert(
        !valid_matches.empty(),
        WaspStage::Semantics,
        "No matching function signature found for " + overload_group_symbol->name
    );

    Doctor::get().assert(
        valid_matches.size() == 1,
        WaspStage::Semantics,
        "Ambiguous function call to " + overload_group_symbol->name
    );

    int index = group_data.get_overload_index(valid_matches.front());

    return {valid_matches.front(), index};
}

std::tuple<Symbol_ptr, int, int> TypeChecker::resolve_method_call(
    SymbolScope_ptr scope,
    const std::string& module_name,
    const std::string& method_name,
    const ObjectVector& argument_types
) const
{
    Symbol_ptr module_symbol = scope->lookup(module_name);

    Doctor::get().fatal_if_nullptr(
        module_symbol,
        WaspStage::Semantics,
        "Module '" + module_name + "' not found in current scope"
    );

    Doctor::get().assert(
        module_symbol->payload_is<ModuleData>(),
        WaspStage::Semantics,
        "Symbol '" + module_name + "' is not a module"
    );

    auto& module_data = module_symbol->get_payload_as<ModuleData>();

    Symbol_ptr overload_group_symbol = module_data.mod->get_member(method_name);
    int member_index = module_data.mod->get_member_index(method_name);

    Doctor::get().fatal_if_nullptr(
        overload_group_symbol,
        WaspStage::Semantics,
        "Method '" + method_name + "' not found in module '" + module_name
    );

    Doctor::get().assert(
        overload_group_symbol->payload_is<OverloadGroupData>(),
        WaspStage::Semantics,
        "Symbol '" + method_name + "' is not an overload group"
    );

    const auto& group_data = overload_group_symbol->get_payload_as<OverloadGroupData>();

    SymbolVector valid_matches;

    collect_assignable_signatures(
        scope,
        group_data.get_all_overloads(),
        argument_types,
        valid_matches
    );

    Doctor::get().assert(
        !valid_matches.empty(),
        WaspStage::Semantics,
        "No matching method signature found for '" + module_name + "." + method_name + "()'"
    );

    Doctor::get().assert(
        valid_matches.size() == 1,
        WaspStage::Semantics,
        "Ambiguous method call to '" + module_name + "." + method_name + "()'"
    );

    int overload_index = group_data.get_overload_index(valid_matches.front());

    return {valid_matches.front(), overload_index, member_index};
}

// ============================================================================
// Equal Checks
// ============================================================================

bool TypeChecker::equal(
    SymbolScope_ptr scope,
    const Object_ptr type_1,
    const Object_ptr type_2
) const
{
    if (!type_1 || !type_2)
        return false;
    if (type_1 == type_2)
        return true;

    if (type_1->is<VariantType>() && type_2->is<VariantType>())
    {
        return equal_unordered(
            scope,
            type_1->as<VariantType>().types,
            type_2->as<VariantType>().types
        );
    }

    return std::visit(
        ::overloaded{
            // Standard Types
            [&](AnyType const&, AnyType const&) -> bool
            {
                return true;
            },
            [&](IntType const&, IntType const&) -> bool
            {
                return true;
            },
            [&](FloatType const&, FloatType const&) -> bool
            {
                return true;
            },
            [&](BooleanType const&, BooleanType const&) -> bool
            {
                return true;
            },
            [&](StringType const&, StringType const&) -> bool
            {
                return true;
            },
            [&](NoneType const&, NoneType const&) -> bool
            {
                return true;
            },

            // Literal Types
            [&](IntLiteralType const& t1, IntLiteralType const& t2) -> bool
            {
                return t1.value == t2.value;
            },
            [&](FloatLiteralType const& t1, FloatLiteralType const& t2) -> bool
            {
                return t1.value == t2.value;
            },
            [&](BooleanLiteralType const& t1, BooleanLiteralType const& t2) -> bool
            {
                return t1.value == t2.value;
            },
            [&](StringLiteralType const& t1, StringLiteralType const& t2) -> bool
            {
                return t1.value == t2.value;
            },

            // Composites
            [&](ListType const& t1, ListType const& t2) -> bool
            {
                return equal(scope, t1.element_type, t2.element_type);
            },
            [&](SetType const& t1, SetType const& t2) -> bool
            {
                return equal(scope, t1.element_type, t2.element_type);
            },
            [&](TupleType const& t1, TupleType const& t2) -> bool
            {
                return equal(scope, t1.element_types, t2.element_types);
            },
            [&](MapType const& t1, MapType const& t2) -> bool
            {
                return equal(scope, t1.key_type, t2.key_type) &&
                       equal(scope, t1.value_type, t2.value_type);
            },

            [](const auto&, const auto&) -> bool
            {
                return false;
            }
        },
        type_1->value,
        type_2->value
    );
}

bool TypeChecker::equal(
    SymbolScope_ptr scope,
    const ObjectVector& type_vector_1,
    const ObjectVector& type_vector_2
) const
{
    if (type_vector_1.size() != type_vector_2.size())
        return false;

    return std::equal(
        type_vector_1.begin(),
        type_vector_1.end(),
        type_vector_2.begin(),
        [&](const auto& a, const auto& b)
        {
            return equal(scope, a, b);
        }
    );
}

bool TypeChecker::equal_unordered(
    SymbolScope_ptr scope,
    const ObjectVector& left_vector,
    const ObjectVector& right_vector
) const
{
    if (left_vector.size() != right_vector.size())
        return false;

    return std::all_of(
        left_vector.begin(),
        left_vector.end(),
        [&](const auto& left)
        {
            return std::any_of(
                right_vector.begin(),
                right_vector.end(),
                [&](const auto& right)
                {
                    return equal(scope, left, right);
                }
            );
        }
    );
}

// ============================================================================
// Assignable Checks (LHS <- RHS)
// ============================================================================

bool TypeChecker::assignable(
    SymbolScope_ptr scope,
    const Object_ptr lhs_type,
    const Object_ptr rhs_type
) const
{
    if (!lhs_type || !rhs_type)
        return false;
    if (equal(scope, lhs_type, rhs_type))
        return true;

    if (rhs_type->is<VariantType>())
    {
        auto& rhs_var = rhs_type->as<VariantType>();
        return std::all_of(
            rhs_var.types.begin(),
            rhs_var.types.end(),
            [&](Object_ptr t)
            {
                return assignable(scope, lhs_type, t);
            }
        );
    }

    if (lhs_type->is<VariantType>())
    {
        auto& lhs_var = lhs_type->as<VariantType>();
        return std::any_of(
            lhs_var.types.begin(),
            lhs_var.types.end(),
            [&](Object_ptr t)
            {
                return assignable(scope, t, rhs_type);
            }
        );
    }

    return std::visit(
        ::overloaded{
            [](AnyType const&, const auto&) -> bool
            {
                return true;
            },
            [](IntType const&, IntType const&) -> bool
            {
                return true;
            },
            [](FloatType const&, FloatType const&) -> bool
            {
                return true;
            },
            [](BooleanType const&, BooleanType const&) -> bool
            {
                return true;
            },
            [](StringType const&, StringType const&) -> bool
            {
                return true;
            },

            // Accept literals
            [](IntType const&, IntLiteralType const&) -> bool
            {
                return true;
            },
            [](FloatType const&, FloatLiteralType const&) -> bool
            {
                return true;
            },
            [](BooleanType const&, BooleanLiteralType const&) -> bool
            {
                return true;
            },
            [](StringType const&, StringLiteralType const&) -> bool
            {
                return true;
            },

            // Handle nested composites
            [&](ListType const& t1, ListType const& t2) -> bool
            {
                return assignable(scope, t1.element_type, t2.element_type);
            },
            [&](SetType const& t1, SetType const& t2) -> bool
            {
                return assignable(scope, t1.element_type, t2.element_type);
            },
            [&](TupleType const& t1, TupleType const& t2) -> bool
            {
                return assignable(scope, t1.element_types, t2.element_types);
            },
            [&](MapType const& t1, MapType const& t2) -> bool
            {
                return assignable(scope, t1.key_type, t2.key_type) &&
                       assignable(scope, t1.value_type, t2.value_type);
            },

            [](const auto&, const auto&) -> bool
            {
                return false;
            }
        },
        lhs_type->value,
        rhs_type->value
    );
}

bool TypeChecker::assignable(
    SymbolScope_ptr scope,
    const ObjectVector& type_vector_1,
    const ObjectVector& type_vector_2
) const
{
    if (type_vector_1.size() != type_vector_2.size())
        return false;

    return std::equal(
        type_vector_1.begin(),
        type_vector_1.end(),
        type_vector_2.begin(),
        [&](const auto& a, const auto& b)
        {
            return assignable(scope, a, b);
        }
    );
}

// ============================================================================
// Inference
// ============================================================================

Object_ptr TypeChecker::infer(
    SymbolScope_ptr scope,
    Object_ptr left_type,
    TokenType op,
    Object_ptr right_type
)
{
    switch (op)
    {
    case TokenType::PLUS:
        if (is_string_type(left_type))
        {
            Doctor::get().assert(
                is_string_type(right_type) || is_number_type(right_type), // Fixed logical AND to OR
                WaspStage::Semantics,
                "Cannot concatenate string with this type"
            );
            return pool->get_string_type();
        }
    case TokenType::STAR:
    case TokenType::POWER:
    case TokenType::MINUS:
    case TokenType::DIVISION:
    case TokenType::MOD: {
        expect_number_type(left_type);
        expect_number_type(right_type);
        return (is_float_type(left_type) || is_float_type(right_type)) ? pool->get_float_type()
                                                                       : pool->get_int_type();
    }
    case TokenType::LESSER_THAN:
    case TokenType::LESSER_THAN_EQUAL:
    case TokenType::GREATER_THAN:
    case TokenType::GREATER_THAN_EQUAL: {
        expect_number_type(left_type);
        expect_number_type(right_type);
        return pool->get_boolean_type();
    }
    case TokenType::EQUAL_EQUAL:
    case TokenType::BANG_EQUAL: {
        if (is_number_type(left_type))
            expect_number_type(right_type);
        else if (is_string_type(left_type))
            expect_string_type(right_type);
        else if (is_boolean_type(left_type))
            expect_boolean_type(right_type);
        else
            Doctor::get().fatal(WaspStage::Semantics, "Unsupported types for equality comparison");
        return pool->get_boolean_type();
    }
    case TokenType::AND:
    case TokenType::OR: {
        expect_boolean_type(left_type);
        expect_boolean_type(right_type);
        return pool->get_boolean_type();
    }
    default:
        Doctor::get().fatal(WaspStage::Semantics, "Unknown binary operator");
    }
    return pool->get_none_type();
}

Object_ptr TypeChecker::infer(SymbolScope_ptr scope, Object_ptr operand_type, TokenType op)
{
    switch (op)
    {
    case TokenType::PLUS:
    case TokenType::MINUS:
        expect_number_type(operand_type);
        return is_int_type(operand_type) ? pool->get_int_type() : pool->get_float_type();
    case TokenType::NOT:
        expect_boolean_type(operand_type);
        return pool->get_boolean_type();
    default:
        Doctor::get().fatal(WaspStage::Semantics, "Unknown unary operator");
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
        ::overloaded{
            [&](ListType const& t)
            {
                return t.element_type;
            },
            [&](TupleType const& t)
            {
                return MAKE_OBJECT_VARIANT(VariantType(t.element_types));
            },
            [&](MapType const& t)
            {
                return MAKE_OBJECT_VARIANT(TupleType({t.key_type, t.value_type}));
            },
            [&](const auto&) -> Object_ptr
            {
                Doctor::get().fatal(WaspStage::Semantics, "Cannot spread a non-iterable type");
                return pool->get_none_type();
            }
        },
        type->value
    );
}

Object_ptr TypeChecker::extract_iterable_element_type(
    SymbolScope_ptr scope,
    const Object_ptr type
) const
{
    if (!type)
        return MAKE_OBJECT_VARIANT(AnyType());

    if (type->is<VariantType>())
    {
        auto& variant = type->as<VariantType>();
        ObjectVector extracted_elements;
        for (const auto& t : variant.types)
            extracted_elements.push_back(extract_iterable_element_type(scope, t));

        ObjectVector unique_elements = remove_duplicates(scope, extracted_elements);
        return unique_elements.size() == 1 ? unique_elements[0]
                                           : MAKE_OBJECT_VARIANT(VariantType(unique_elements));
    }

    return std::visit(
        ::overloaded{
            [](ListType const& t) -> Object_ptr
            {
                return t.element_type;
            },
            [](SetType const& t) -> Object_ptr
            {
                return t.element_type;
            },
            [](MapType const& t) -> Object_ptr
            {
                return MAKE_OBJECT_VARIANT(TupleType({t.key_type, t.value_type}));
            },
            [&](TupleType const& t) -> Object_ptr
            {
                ObjectVector unique = remove_duplicates(scope, t.element_types);
                return unique.size() == 1 ? unique[0] : MAKE_OBJECT_VARIANT(VariantType(unique));
            },
            [&](StringType const&) -> Object_ptr
            {
                return pool->get_string_type();
            },
            [&](const auto&) -> Object_ptr
            {
                return pool->get_any_type();
            }
        },
        type->value
    );
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
        ::overloaded{
            [](BooleanType const&)
            {
                return true;
            },
            [](BooleanLiteralType const&)
            {
                return true;
            },
            [](StringType const&)
            {
                return true;
            },
            [](StringLiteralType const&)
            {
                return true;
            },
            [](ListType const&)
            {
                return true;
            },
            [](TupleType const&)
            {
                return true;
            },
            [](SetType const&)
            {
                return true;
            },
            [](MapType const&)
            {
                return true;
            },
            [&](VariantType const& t)
            {
                return std::all_of(
                    t.types.begin(),
                    t.types.end(),
                    [&](Object_ptr o)
                    {
                        return is_condition_type(scope, o);
                    }
                );
            },
            [](const auto&)
            {
                return false;
            }
        },
        condition_type->value
    );
}

bool TypeChecker::is_spreadable_type(SymbolScope_ptr scope, const Object_ptr candidate_type) const
{
    if (!candidate_type)
        return false;
    return std::visit(
        ::overloaded{
            [&](ListType const&)
            {
                return true;
            },
            [&](TupleType const&)
            {
                return true;
            },
            [&](MapType const&)
            {
                return true;
            },
            [&](VariantType const& t)
            {
                return std::all_of(
                    t.types.begin(),
                    t.types.end(),
                    [&](Object_ptr o)
                    {
                        return is_spreadable_type(scope, o);
                    }
                );
            },
            [](const auto&)
            {
                return false;
            }
        },
        candidate_type->value
    );
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
        "Expected a valid condition type (boolean)"
    );
}
void TypeChecker::expect_spreadable_type(SymbolScope_ptr scope, const Object_ptr type) const
{
    Doctor::get().assert(
        is_spreadable_type(scope, type),
        WaspStage::Semantics,
        "Expected a spreadable collection type"
    );
}
void TypeChecker::expect_iterable_type(SymbolScope_ptr scope, const Object_ptr type) const
{
    Doctor::get()
        .assert(is_iterable_type(scope, type), WaspStage::Semantics, "Expected an iterable type");
}
void TypeChecker::expect_key_type(SymbolScope_ptr scope, const Object_ptr type) const
{
    Doctor::get().assert(
        is_key_type(scope, type),
        WaspStage::Semantics,
        "Type cannot be used as a Dictionary/Map key"
    );
}

// ============================================================================
// Type Utilities
// ============================================================================

bool TypeChecker::any_eq(SymbolScope_ptr scope, const ObjectVector& vec, const Object_ptr x) const
{
    return std::any_of(
        vec.begin(),
        vec.end(),
        [&](const auto& item)
        {
            return equal(scope, item, x);
        }
    );
}

ObjectVector TypeChecker::remove_duplicates(SymbolScope_ptr scope, const ObjectVector& vec) const
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
