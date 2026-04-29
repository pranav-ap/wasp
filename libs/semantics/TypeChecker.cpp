#include "TypeChecker.h"
#include "Doctor.h"
#include "Objects.h"
#include "SymbolScope.h"
#include "Token.h"

#include <algorithm>
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

    if (type_1->is<GenericType_ptr>() && type_2->is<GenericType_ptr>())
    {
        auto g1 = type_1->as<GenericType_ptr>();
        auto g2 = type_2->as<GenericType_ptr>();
        return equal(scope, g1->constraint_type, g2->constraint_type);
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
            [&](EnumType_ptr const& t1, EnumType_ptr const& t2) -> bool
            {
                return t1->name == t2->name;
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

    if (rhs_type->is<GenericType_ptr>())
    {
        return assignable(scope, lhs_type, rhs_type->as<GenericType_ptr>()->constraint_type);
    }

    if (lhs_type->is<GenericType_ptr>())
    {
        return assignable(scope, lhs_type->as<GenericType_ptr>()->constraint_type, rhs_type);
    }

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
        overloaded{
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

Object_ptr TypeChecker::infer(SymbolScope_ptr scope, Object_ptr operand_type, TokenType op)
{
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

} // namespace Wasp
