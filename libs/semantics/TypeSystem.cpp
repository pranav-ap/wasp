#include "TypeSystem.h"
#include "Doctor.h"
#include "Objects.h"
#include "SymbolScope.h"
#include "Token.h"
#include "Workspace.h"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
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
// Equal Checks
// ============================================================================

bool TypeSystem::equal(
    SymbolScope_ptr scope,
    const Object_ptr type_1,
    const Object_ptr type_2
) const
{
    if (!type_1 || !type_2)
        return false;

    if (type_1 == type_2)
        return true;

    Object_ptr t1 = type_1;
    Object_ptr t2 = type_2;

    if (t1->is<TypeAlias_ptr>())
    {
        t1 = t1->as<TypeAlias_ptr>()->underlying_type;
    }
    if (t2->is<TypeAlias_ptr>())
    {
        t2 = t2->as<TypeAlias_ptr>()->underlying_type;
    }

    if (t1 == t2)
        return true;

    if (t1->is<VariantType>() && t2->is<VariantType>())
    {
        return equal_unordered(scope, t1->as<VariantType>().types, t2->as<VariantType>().types);
    }

    if (t1->is<GenericType_ptr>() && t2->is<GenericType_ptr>())
    {
        auto g1 = t1->as<GenericType_ptr>();
        auto g2 = t2->as<GenericType_ptr>();
        return equal(scope, g1->constraint_type, g2->constraint_type);
    }

    return std::visit(
        ::overloaded{
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

            [&](IntLiteralType const& l1, IntLiteralType const& l2) -> bool
            {
                return l1.value == l2.value;
            },
            [&](FloatLiteralType const& l1, FloatLiteralType const& l2) -> bool
            {
                return l1.value == l2.value;
            },
            [&](BooleanLiteralType const& l1, BooleanLiteralType const& l2) -> bool
            {
                return l1.value == l2.value;
            },
            [&](StringLiteralType const& l1, StringLiteralType const& l2) -> bool
            {
                return l1.value == l2.value;
            },

            [&](ListType const& l1, ListType const& l2) -> bool
            {
                return equal(scope, l1.element_type, l2.element_type);
            },
            [&](SetType const& s1, SetType const& s2) -> bool
            {
                return equal(scope, s1.element_type, s2.element_type);
            },
            [&](TupleType const& t_1, TupleType const& t_2) -> bool
            {
                return equal(scope, t_1.element_types, t_2.element_types);
            },
            [&](MapType const& m1, MapType const& m2) -> bool
            {
                return equal(scope, m1.key_type, m2.key_type) &&
                       equal(scope, m1.value_type, m2.value_type);
            },

            // Structured Types
            [&](Signature_ptr const& s1, Signature_ptr const& s2) -> bool
            {
                return equal(scope, s1->parameter_types, s2->parameter_types) &&
                       equal(scope, s1->return_type, s2->return_type);
            },
            [&](ClassType_ptr const& c1, ClassType_ptr const& c2) -> bool
            {
                return c1->name == c2->name;
            },
            [&](TraitType_ptr const& t1, TraitType_ptr const& t2) -> bool
            {
                return t1->name == t2->name;
            },
            [&](ModuleType_ptr const& m1, ModuleType_ptr const& m2) -> bool
            {
                return m1->name == m2->name;
            },
            [&](TemplateType_ptr const& t1, TemplateType_ptr const& t2) -> bool
            {
                return equal(scope, t1->underlying_type, t2->underlying_type);
            },
            [&](EnumType_ptr const& e1, EnumType_ptr const& e2) -> bool
            {
                auto get_root = [](const std::string& name)
                {
                    size_t pos = name.find('.');
                    return pos == std::string::npos ? name : name.substr(0, pos);
                };
                return get_root(e1->name) == get_root(e2->name);
            },

            [](const auto&, const auto&) -> bool
            {
                return false;
            }
        },
        t1->value,
        t2->value
    );
}

bool TypeSystem::equal(
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

bool TypeSystem::equal_unordered(
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

bool TypeSystem::assignable(
    SymbolScope_ptr scope,
    const Object_ptr lhs_type,
    const Object_ptr rhs_type
) const
{
    if (!lhs_type || !rhs_type)
        return false;

    // Resolve Aliases
    if (lhs_type->is<TypeAlias_ptr>())
    {
        return assignable(scope, lhs_type->as<TypeAlias_ptr>()->underlying_type, rhs_type);
    }

    if (rhs_type->is<TypeAlias_ptr>())
    {
        return assignable(scope, lhs_type, rhs_type->as<TypeAlias_ptr>()->underlying_type);
    }

    // Exact Matches (Catches Nominals like ClassType, TraitType, ModuleType, EnumType, and
    // Signatures)
    if (equal(scope, lhs_type, rhs_type))
        return true;

    // Generics
    if (rhs_type->is<GenericType_ptr>())
    {
        return assignable(scope, lhs_type, rhs_type->as<GenericType_ptr>()->constraint_type);
    }

    if (lhs_type->is<GenericType_ptr>())
    {
        return assignable(scope, lhs_type->as<GenericType_ptr>()->constraint_type, rhs_type);
    }

    // Variants
    if (rhs_type->is<VariantType>())
    {
        auto rhs_var = rhs_type->as<VariantType>();
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
        auto lhs_var = lhs_type->as<VariantType>();
        return std::any_of(
            lhs_var.types.begin(),
            lhs_var.types.end(),
            [&](Object_ptr t)
            {
                return assignable(scope, t, rhs_type);
            }
        );
    }

    // Primitive & Collection Matching
    return std::visit(
        overloaded{
            [](AnyType const&, const auto&) -> bool
            {
                return true;
            },

            // Base to Base (let a: int = b)
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

            // Literal to Literal (let a: 'sam' = 'sam')
            // This ensures 'sam' = 'tim' correctly evaluates to false!
            [](IntLiteralType const& l, IntLiteralType const& r) -> bool
            {
                return l.value == r.value;
            },
            [](FloatLiteralType const& l, FloatLiteralType const& r) -> bool
            {
                return l.value == r.value;
            },
            [](BooleanLiteralType const& l, BooleanLiteralType const& r) -> bool
            {
                return l.value == r.value;
            },
            [](StringLiteralType const& l, StringLiteralType const& r) -> bool
            {
                return l.value == r.value;
            },

            // Literal to Base (let a: int = 34)
            // (LHS is Base, RHS is Literal)
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

            // Collections
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

            // Fallback for everything else (including unsafe Base-to-Literal assignments)
            [](const auto&, const auto&) -> bool
            {
                return false;
            }
        },
        lhs_type->value,
        rhs_type->value
    );
}

bool TypeSystem::assignable(
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

Object_ptr TypeSystem::infer(
    SymbolScope_ptr scope,
    Object_ptr left_type,
    TokenType op,
    Object_ptr right_type
)
{
    if (left_type->is<TypeAlias_ptr>())
    {
        left_type = left_type->as<TypeAlias_ptr>()->underlying_type;
    }
    if (right_type->is<TypeAlias_ptr>())
    {
        right_type = right_type->as<TypeAlias_ptr>()->underlying_type;
    }

    if (left_type->is<GenericType_ptr>())
    {
        left_type = left_type->as<GenericType_ptr>()->constraint_type;
    }

    if (right_type->is<GenericType_ptr>())
    {
        right_type = right_type->as<GenericType_ptr>()->constraint_type;
    }

    if (left_type->is<VariantType>())
    {
        ObjectVector result_types;
        for (const auto& t : left_type->as<VariantType>().types)
        {
            result_types.push_back(infer(scope, t, op, right_type));
        }
        return std::make_shared<Object>(VariantType{result_types});
    }

    if (right_type->is<VariantType>())
    {
        ObjectVector result_types;
        for (const auto& t : right_type->as<VariantType>().types)
        {
            result_types.push_back(infer(scope, left_type, op, t));
        }
        return std::make_shared<Object>(VariantType{result_types});
    }

    switch (op)
    {
    case TokenType::PLUS: {
        if (is_string_type(left_type) || is_string_type(right_type))
        {
            bool left_valid = is_string_type(left_type) || is_number_type(left_type);
            bool right_valid = is_string_type(right_type) || is_number_type(right_type);

            if (!left_valid)
            {
                Doctor::get().fatal(
                    WaspStage::Semantics,
                    "Invalid string concatenation: Left operand is '" +
                        stringify_object(left_type) + "', expected a string or number."
                );
            }

            if (!right_valid)
            {
                Doctor::get().fatal(
                    WaspStage::Semantics,
                    "Invalid string concatenation: Right operand is '" +
                        stringify_object(right_type) + "', expected a string or number."
                );
            }

            return pool->get_string_type();
        }
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
        if (left_type->is<NoneType>() || right_type->is<NoneType>())
            return pool->get_boolean_type();

        if (is_number_type(left_type))
            expect_number_type(right_type);
        else if (is_string_type(left_type))
            expect_string_type(right_type);
        else if (is_boolean_type(left_type))
            expect_boolean_type(right_type);
        else if (left_type->is<EnumType_ptr>())
        {
            Doctor::get().assert(
                equal(scope, left_type, right_type),
                WaspStage::Semantics,
                "Type mismatch in equality comparison: cannot compare enum '" +
                    stringify_object(left_type) + "' with '" + stringify_object(right_type) + "'."
            );
        }
        else
            Doctor::get().fatal(
                WaspStage::Semantics,
                "Type mismatch in equality comparison: cannot compare '" +
                    stringify_object(left_type) + "' with '" + stringify_object(right_type) + "'."
            );
        return pool->get_boolean_type();
    }
    case TokenType::AND:
    case TokenType::OR: {
        expect_boolean_type(left_type);
        expect_boolean_type(right_type);
        return pool->get_boolean_type();
    }
    default:
        Doctor::get().fatal(
            WaspStage::Semantics,
            "Unsupported binary operator for types '" + stringify_object(left_type) + "' and '" +
                stringify_object(right_type) + "'."
        );
    }
    return pool->get_none_type();
}

Object_ptr TypeSystem::infer(SymbolScope_ptr scope, Object_ptr operand_type, TokenType op)
{
    if (operand_type->is<TypeAlias_ptr>())
    {
        operand_type = operand_type->as<TypeAlias_ptr>()->underlying_type;
    }

    if (operand_type->is<GenericType_ptr>())
    {
        operand_type = operand_type->as<GenericType_ptr>()->constraint_type;
    }

    if (operand_type->is<VariantType>())
    {
        ObjectVector result_types;
        for (const auto& t : operand_type->as<VariantType>().types)
        {
            result_types.push_back(infer(scope, t, op));
        }
        return std::make_shared<Object>(VariantType{result_types});
    }

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

Signature_ptr TypeSystem::get_function_signature(Object_ptr type_obj) const
{
    if (!type_obj)
    {
        return nullptr;
    }

    if (auto p = type_obj->try_as<Signature_ptr>())
    {
        return *p;
    }

    if (auto p = type_obj->try_as<TemplateType_ptr>())
    {
        if (auto sig = (*p)->underlying_type->try_as<Signature_ptr>())
        {
            return *sig;
        }
    }

    return nullptr;
}

Object_ptr TypeSystem::spread_type(Object_ptr type)
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
                return make_object(VariantType(t.element_types));
            },
            [&](MapType const& t)
            {
                return make_object(TupleType(ObjectVector{t.key_type, t.value_type}));
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

Object_ptr TypeSystem::extract_iterable_element_type(
    SymbolScope_ptr scope,
    const Object_ptr type
) const
{
    if (!type)
        return make_object(AnyType());

    if (type->is<VariantType>())
    {
        auto& variant = type->as<VariantType>();
        ObjectVector extracted_elements;
        for (const auto& t : variant.types)
            extracted_elements.push_back(extract_iterable_element_type(scope, t));

        ObjectVector unique_elements = remove_duplicates(scope, extracted_elements);
        return unique_elements.size() == 1 ? unique_elements[0]
                                           : make_object(VariantType(unique_elements));
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
                return make_object(TupleType(ObjectVector{t.key_type, t.value_type}));
            },
            [&](TupleType const& t) -> Object_ptr
            {
                ObjectVector unique = remove_duplicates(scope, t.element_types);
                return unique.size() == 1 ? unique[0] : make_object(VariantType(unique));
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

bool TypeSystem::is_int_type(Object_ptr obj) const
{
    return obj->is<IntType>() || obj->is<IntLiteralType>();
}

bool TypeSystem::is_float_type(Object_ptr obj) const
{
    return obj->is<FloatType>() || obj->is<FloatLiteralType>();
}

bool TypeSystem::is_number_type(Object_ptr obj) const
{
    return is_int_type(obj) || is_float_type(obj);
}

bool TypeSystem::is_string_type(Object_ptr obj) const
{
    return obj->is<StringType>() || obj->is<StringLiteralType>();
}

bool TypeSystem::is_boolean_type(Object_ptr obj) const
{
    return obj->is<BooleanType>() || obj->is<BooleanLiteralType>();
}

bool TypeSystem::is_none_type(const Object_ptr type) const
{
    return type && std::holds_alternative<NoneType>(type->value);
}

bool TypeSystem::is_condition_type(SymbolScope_ptr scope, const Object_ptr condition_type) const
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

bool TypeSystem::is_spreadable_type(SymbolScope_ptr scope, const Object_ptr candidate_type) const
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

bool TypeSystem::is_iterable_type(SymbolScope_ptr scope, const Object_ptr candidate_type) const
{
    return is_string_type(candidate_type) || is_spreadable_type(scope, candidate_type);
}

bool TypeSystem::is_key_type(SymbolScope_ptr scope, const Object_ptr key_type) const
{
    return is_int_type(key_type) || is_float_type(key_type) || is_string_type(key_type) ||
           is_boolean_type(key_type);
}

void TypeSystem::expect_boolean_type(const Object_ptr type) const
{
    Doctor::get().assert(is_boolean_type(type), WaspStage::Semantics, "Expected a boolean type");
}

void TypeSystem::expect_number_type(const Object_ptr type) const
{
    Doctor::get().assert(is_number_type(type), WaspStage::Semantics, "Expected a number type");
}

void TypeSystem::expect_int_type(const Object_ptr type) const
{
    Doctor::get().assert(is_int_type(type), WaspStage::Semantics, "Expected an integer type");
}

void TypeSystem::expect_float_type(const Object_ptr type) const
{
    Doctor::get().assert(is_float_type(type), WaspStage::Semantics, "Expected a float type");
}

void TypeSystem::expect_string_type(const Object_ptr type) const
{
    Doctor::get().assert(is_string_type(type), WaspStage::Semantics, "Expected a string type");
}

void TypeSystem::expect_none_type(const Object_ptr type) const
{
    Doctor::get().assert(is_none_type(type), WaspStage::Semantics, "Expected a None type");
}

void TypeSystem::expect_condition_type(SymbolScope_ptr scope, const Object_ptr type) const
{
    Doctor::get().assert(
        is_condition_type(scope, type),
        WaspStage::Semantics,
        "Expected a valid condition type (boolean)"
    );
}

void TypeSystem::expect_spreadable_type(SymbolScope_ptr scope, const Object_ptr type) const
{
    Doctor::get().assert(
        is_spreadable_type(scope, type),
        WaspStage::Semantics,
        "Expected a spreadable collection type"
    );
}

void TypeSystem::expect_iterable_type(SymbolScope_ptr scope, const Object_ptr type) const
{
    Doctor::get()
        .assert(is_iterable_type(scope, type), WaspStage::Semantics, "Expected an iterable type");
}

void TypeSystem::expect_key_type(SymbolScope_ptr scope, const Object_ptr type) const
{
    Doctor::get().assert(
        is_key_type(scope, type),
        WaspStage::Semantics,
        "Type cannot be used as a Dictionary/Map key"
    );
}

bool TypeSystem::any_eq(SymbolScope_ptr scope, const ObjectVector& vec, const Object_ptr x) const
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

ObjectVector TypeSystem::remove_duplicates(SymbolScope_ptr scope, const ObjectVector& vec) const
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

SymbolVector::iterator TypeSystem::find_matching_signature(
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
            auto existing_signature = get_function_signature(sym->get_type());

            // Skip unresolved functions or non-functions
            if (!existing_signature)
                return false;

            if (existing_signature->parameter_types.size() != parameter_types.size())
                return false;

            for (size_t i = 0; i < parameter_types.size(); ++i)
            {
                if (!equal(scope, existing_signature->parameter_types[i], parameter_types[i]))
                    return false;
            }

            return true;
        }
    );
}

void TypeSystem::validate_new_function_overload(
    SymbolScope_ptr scope,
    std::string& function_name,
    const Symbol_ptr new_function_symbol
)
{
    auto existing_symbol = scope->lookup(function_name);

    if (!existing_symbol)
    {
        return;
    }

    auto new_type = new_function_symbol->get_type();
    auto new_function_signature = get_function_signature(new_type);

    Doctor::get().fatal_if_nullptr(
        new_function_signature,
        WaspStage::Semantics,
        "New function symbol lacks a valid function type"
    );

    const auto& parameter_types = new_function_signature->parameter_types;

    bool new_is_template = false;
    size_t new_generic_count = 0;

    if (auto t = new_type->try_as<TemplateType_ptr>())
    {
        if ((*t)->underlying_type->is<Signature_ptr>())
        {
            new_is_template = true;
            new_generic_count = (*t)->generics.size();
        }
    }

    // Helper to check against a single existing symbol
    auto check_duplicate = [&](const Symbol_ptr& sibling)
    {
        if (sibling == new_function_symbol)
            return;

        auto type_obj = sibling->get_type();
        if (!type_obj)
            return;

        auto existing_signature = get_function_signature(type_obj);

        Doctor::get().assert(
            existing_signature != nullptr,
            WaspStage::Semantics,
            "Existing symbol does not contain a valid function signature"
        );

        bool existing_is_template = false;
        size_t existing_generic_count = 0;

        if (auto t = type_obj->try_as<TemplateType_ptr>())
        {
            if ((*t)->underlying_type->is<Signature_ptr>())
            {
                existing_is_template = true;
                existing_generic_count = (*t)->generics.size();
            }
        }

        bool signatures_match = equal(scope, existing_signature->parameter_types, parameter_types);
        bool templates_match = (new_is_template == existing_is_template) &&
                               (new_generic_count == existing_generic_count);

        Doctor::get().assert(
            !(signatures_match && templates_match),
            WaspStage::Semantics,
            "Duplicate function signatures for '" + function_name + "' defined in same scope"
        );
    };

    if (existing_symbol->payload_is<OverloadsData>())
    {
        auto& group_data = existing_symbol->get_payload_as<OverloadsData>();

        // Sibling Duplicate Check
        for (const auto& sibling : group_data.overloads)
        {
            check_duplicate(sibling);
        }

        // Shadow Parents
        auto parent_match = std::find_if(
            group_data.parents.begin(),
            group_data.parents.end(),
            [&](const Symbol_ptr& parent)
            {
                auto parent_type = parent->get_type();
                if (!parent_type)
                    return false;

                auto parent_signature = get_function_signature(parent_type);
                if (!parent_signature)
                    return false;

                bool parent_is_template = false;
                size_t parent_generic_count = 0;

                if (auto t = parent_type->try_as<TemplateType_ptr>())
                {
                    if ((*t)->underlying_type->is<Signature_ptr>())
                    {
                        parent_is_template = true;
                        parent_generic_count = (*t)->generics.size();
                    }
                }

                return (parent_is_template == new_is_template) &&
                       (parent_generic_count == new_generic_count) &&
                       equal(scope, parent_signature->parameter_types, parameter_types);
            }
        );

        if (parent_match != group_data.parents.end())
        {
            group_data.parents.erase(parent_match);
        }
    }
    else if (
        existing_symbol->payload_is<TemplateData>() || existing_symbol->payload_is<FunctionData>()
    )
    {
        // If it's a raw symbol that hasn't been grouped yet, just check against it directly
        check_duplicate(existing_symbol);
    }
    else
    {
        Doctor::get().fatal(
            WaspStage::Semantics,
            "Symbol '" + function_name + "' is not a function or template and cannot be overloaded."
        );
    }
}

void TypeSystem::validate_new_method_overload(
    SymbolScope_ptr scope,
    ObjectVector existing_overloads,
    const Symbol_ptr new_method_symbol
)
{
    auto new_method_signature = get_function_signature(new_method_symbol->get_type());

    Doctor::get().fatal_if_nullptr(
        new_method_signature,
        WaspStage::Semantics,
        "New method symbol lacks a valid function type"
    );

    const auto& parameter_types = new_method_signature->parameter_types;

    for (const auto& existing_overload : existing_overloads)
    {
        auto existing_signature = get_function_signature(existing_overload);

        Doctor::get().assert(
            existing_signature != nullptr,
            WaspStage::Semantics,
            "Overload group contains a non-function symbol"
        );

        if (equal(scope, existing_signature->parameter_types, parameter_types))
        {
            Doctor::get().fatal(
                WaspStage::Semantics,
                "Duplicate method signatures for '" + new_method_symbol->name +
                    "' defined in same class"
            );
        }
    }
}

std::tuple<Symbol_ptr, int> TypeSystem::get_best_function_signature(
    SymbolScope_ptr scope,
    const SymbolVector& candidates,
    const ObjectVector& argument_types
) const
{
    SymbolVector valid_matches;
    std::vector<int> match_indices;

    for (size_t i = 0; i < candidates.size(); ++i)
    {
        auto signature = get_function_signature(candidates[i]->get_type());

        if (!signature)
            continue;

        if (assignable(scope, signature->parameter_types, argument_types))
        {
            valid_matches.push_back(candidates[i]);
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

    return {valid_matches.front(), match_indices.front()};
}

Object_ptr TypeSystem::substitute_generics(
    Object_ptr type,
    TemplateType_ptr template_type,
    const ObjectVector& generic_args
) const
{
    if (!type)
        return nullptr;

    auto sub = [&](Object_ptr t)
    {
        return substitute_generics(t, template_type, generic_args);
    };

    auto sub_all = [&](const ObjectVector& types)
    {
        ObjectVector results;
        results.reserve(types.size());

        for (const auto& t : types)
        {
            results.push_back(sub(t));
        }

        return results;
    };

    auto result = std::visit(
        overloaded{
            [&](GenericType_ptr g) -> Object_ptr
            {
                if (auto it = template_type->generics.find(g->name);
                    it != template_type->generics.end())
                {
                    return generic_args[std::distance(template_type->generics.begin(), it)];
                }

                Doctor::get().fatal(WaspStage::Semantics, "Generic '" + g->name + "' not found.");
            },
            [&](TypeAlias_ptr t)
            {
                return sub(t->underlying_type);
            },
            [&](ListType& t)
            {
                return make_object(ListType{sub(t.element_type)});
            },
            [&](SetType& t)
            {
                return make_object(SetType{sub(t.element_type)});
            },
            [&](MapType& t)
            {
                return make_object(MapType{sub(t.key_type), sub(t.value_type)});
            },
            [&](TupleType& t)
            {
                return make_object(TupleType{sub_all(t.element_types)});
            },
            [&](VariantType& t)
            {
                return make_object(VariantType{sub_all(t.types)});
            },
            [&](Signature_ptr t)
            {
                return make_object(
                    std::make_shared<Signature>(sub_all(t->parameter_types), sub(t->return_type))
                );
            },
            [&](ObjectOverloadList_ptr t)
            {
                auto list = std::make_shared<ObjectOverloadList>();
                list->overloads = sub_all(t->overloads);
                return make_object(list);
            },
            [&](ClassType_ptr t) -> Object_ptr
            {
                ObjectStringMap concrete_members;

                for (auto const& [name, member_type] : t->members)
                {
                    concrete_members[name] = sub(member_type);
                }

                return make_object(
                    std::make_shared<ClassType>(
                        t->name,
                        std::move(concrete_members),
                        t->fields,
                        t->methods,
                        t->pures,
                        t->statics
                    )
                );
            },
            [](auto&) -> Object_ptr
            {
                return nullptr;
            }
        },
        type->value
    );

    return result ? result : type;
}

Object_ptr TypeSystem::substitute_generics(
    TemplateType_ptr template_type,
    const ObjectVector& generic_args
) const
{
    auto underlying_sig = template_type->underlying_type->as<Signature_ptr>();

    ObjectVector concrete_param_types;
    for (const auto& param_type : underlying_sig->parameter_types)
    {
        concrete_param_types.push_back(
            substitute_generics(param_type, template_type, generic_args)
        );
    }

    Object_ptr concrete_return_type = substitute_generics(
        underlying_sig->return_type,
        template_type,
        generic_args
    );

    return make_object(
        std::make_shared<Signature>(Signature{concrete_param_types, concrete_return_type})
    );
}

} // namespace Wasp
