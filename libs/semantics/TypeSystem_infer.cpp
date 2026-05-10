#include "Doctor.h"
#include "Objects.h"
#include "SymbolScope.h"
#include "Token.h"
#include "TypeSystem.h"

#include <algorithm>
#include <cstddef>
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
// Equality Checks
// ============================================================================

bool TypeSystem::equal(
    SymbolScope_ptr scope,
    const Object_ptr type_1,
    const Object_ptr type_2
) const
{
    if (!type_1 || !type_2)
    {
        return false;
    }
    if (type_1 == type_2)
    {
        return true;
    }

    Object_ptr t1 = type_1;
    Object_ptr t2 = type_2;

    // Resolve Aliases
    if (t1->is<TypeAlias_ptr>())
    {
        t1 = t1->as<TypeAlias_ptr>()->underlying_type;
    }
    if (t2->is<TypeAlias_ptr>())
    {
        t2 = t2->as<TypeAlias_ptr>()->underlying_type;
    }

    if (t1 == t2)
    {
        return true;
    }

    // Special Case: Variants
    if (t1->is<VariantType>() && t2->is<VariantType>())
    {
        return equal_unordered(
            scope,
            t1->as<VariantType>().types,
            t2->as<VariantType>().types
        );
    }

    // Special Case: Generics
    if (t1->is<GenericType_ptr>() && t2->is<GenericType_ptr>())
    {
        auto g1 = t1->as<GenericType_ptr>();
        auto g2 = t2->as<GenericType_ptr>();
        return equal(scope, g1->constraint_type, g2->constraint_type);
    }

    return std::visit(
        ::overloaded{
            [&](AnyType const&, AnyType const&)
            {
                return true;
            },
            [&](IntType const&, IntType const&)
            {
                return true;
            },
            [&](FloatType const&, FloatType const&)
            {
                return true;
            },
            [&](BooleanType const&, BooleanType const&)
            {
                return true;
            },
            [&](StringType const&, StringType const&)
            {
                return true;
            },
            [&](NoneType const&, NoneType const&)
            {
                return true;
            },

            [&](IntLiteralType const& l1, IntLiteralType const& l2)
            {
                return l1.value == l2.value;
            },
            [&](FloatLiteralType const& l1, FloatLiteralType const& l2)
            {
                return l1.value == l2.value;
            },
            [&](BooleanLiteralType const& l1, BooleanLiteralType const& l2)
            {
                return l1.value == l2.value;
            },
            [&](StringLiteralType const& l1, StringLiteralType const& l2)
            {
                return l1.value == l2.value;
            },

            [&](ListType const& l1, ListType const& l2)
            {
                return equal(scope, l1.element_type, l2.element_type);
            },
            [&](SetType const& s1, SetType const& s2)
            {
                return equal(scope, s1.element_type, s2.element_type);
            },
            [&](TupleType const& t1, TupleType const& t2)
            {
                return equal(scope, t1.element_types, t2.element_types);
            },
            [&](MapType const& m1, MapType const& m2)
            {
                return equal(scope, m1.key_type, m2.key_type) &&
                       equal(scope, m1.value_type, m2.value_type);
            },

            [&](Signature_ptr const& s1, Signature_ptr const& s2)
            {
                return equal(scope, s1->parameter_types, s2->parameter_types) &&
                       equal(scope, s1->return_type, s2->return_type);
            },
            [&](ClassType_ptr const& c1, ClassType_ptr const& c2)
            {
                return c1->name == c2->name;
            },
            [&](TraitType_ptr const& t1, TraitType_ptr const& t2)
            {
                return t1->name == t2->name;
            },
            [&](ModuleType_ptr const& m1, ModuleType_ptr const& m2)
            {
                return m1->name == m2->name;
            },
            [&](EnumType_ptr const& e1, EnumType_ptr const& e2)
            {
                auto get_root = [](const std::string& name)
                {
                    size_t pos = name.find('.');
                    return pos == std::string::npos ? name : name.substr(0, pos);
                };
                return get_root(e1->name) == get_root(e2->name);
            },

            [](const auto&, const auto&)
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
    {
        return false;
    }
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
    {
        return false;
    }
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
// Assignability Checks (LHS <- RHS)
// ============================================================================

bool TypeSystem::assignable(
    SymbolScope_ptr scope,
    const Object_ptr lhs_type,
    const Object_ptr rhs_type
) const
{
    if (!lhs_type || !rhs_type)
    {
        return false;
    }

    // Resolve Aliases
    if (lhs_type->is<TypeAlias_ptr>())
    {
        return assignable(
            scope,
            lhs_type->as<TypeAlias_ptr>()->underlying_type,
            rhs_type
        );
    }
    if (rhs_type->is<TypeAlias_ptr>())
    {
        return assignable(
            scope,
            lhs_type,
            rhs_type->as<TypeAlias_ptr>()->underlying_type
        );
    }

    if (equal(scope, lhs_type, rhs_type))
    {
        return true;
    }

    // Generics
    if (rhs_type->is<GenericType_ptr>())
    {
        return assignable(
            scope,
            lhs_type,
            rhs_type->as<GenericType_ptr>()->constraint_type
        );
    }
    if (lhs_type->is<GenericType_ptr>())
    {
        return assignable(
            scope,
            lhs_type->as<GenericType_ptr>()->constraint_type,
            rhs_type
        );
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

    return std::visit(
        overloaded{
            [](AnyType const&, const auto&)
            {
                return true;
            },
            [](IntType const&, IntType const&)
            {
                return true;
            },
            [](FloatType const&, FloatType const&)
            {
                return true;
            },
            [](BooleanType const&, BooleanType const&)
            {
                return true;
            },
            [](StringType const&, StringType const&)
            {
                return true;
            },

            [](IntLiteralType const& l, IntLiteralType const& r)
            {
                return l.value == r.value;
            },
            [](FloatLiteralType const& l, FloatLiteralType const& r)
            {
                return l.value == r.value;
            },
            [](BooleanLiteralType const& l, BooleanLiteralType const& r)
            {
                return l.value == r.value;
            },
            [](StringLiteralType const& l, StringLiteralType const& r)
            {
                return l.value == r.value;
            },

            [](IntType const&, IntLiteralType const&)
            {
                return true;
            },
            [](FloatType const&, FloatLiteralType const&)
            {
                return true;
            },
            [](BooleanType const&, BooleanLiteralType const&)
            {
                return true;
            },
            [](StringType const&, StringLiteralType const&)
            {
                return true;
            },

            [&](ListType const& t1, ListType const& t2)
            {
                return assignable(scope, t1.element_type, t2.element_type);
            },
            [&](SetType const& t1, SetType const& t2)
            {
                return assignable(scope, t1.element_type, t2.element_type);
            },
            [&](TupleType const& t1, TupleType const& t2)
            {
                return assignable(scope, t1.element_types, t2.element_types);
            },
            [&](MapType const& t1, MapType const& t2)
            {
                return assignable(scope, t1.key_type, t2.key_type) &&
                       assignable(scope, t1.value_type, t2.value_type);
            },

            [](const auto&, const auto&)
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
    {
        return false;
    }
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
    case TokenType::PLUS:
        if (is_string_type(left_type) || is_string_type(right_type))
        {
            bool left_valid = is_string_type(left_type) || is_number_type(left_type);
            bool right_valid = is_string_type(right_type) ||
                               is_number_type(right_type);
            if (!left_valid)
            {
                Doctor::get().fatal(
                    WaspStage::Semantics,
                    "Invalid concatenation: Left is '" +
                        stringify_object(left_type) + "'"
                );
            }
            if (!right_valid)
            {
                Doctor::get().fatal(
                    WaspStage::Semantics,
                    "Invalid concatenation: Right is '" +
                        stringify_object(right_type) + "'"
                );
            }
            return pool->get_native_string_type();
        }
        [[fallthrough]];
    case TokenType::STAR:
    case TokenType::POWER:
    case TokenType::MINUS:
    case TokenType::DIVISION:
    case TokenType::MOD:
        expect_number_type(left_type);
        expect_number_type(right_type);
        return (is_float_type(left_type) || is_float_type(right_type))
                   ? pool->get_native_float_type()
                   : pool->get_native_int_type();

    case TokenType::LESSER_THAN:
    case TokenType::LESSER_THAN_EQUAL:
    case TokenType::GREATER_THAN:
    case TokenType::GREATER_THAN_EQUAL:
        expect_number_type(left_type);
        expect_number_type(right_type);
        return pool->get_boolean_type();

    case TokenType::EQUAL_EQUAL:
    case TokenType::BANG_EQUAL:
        if (left_type->is<NoneType>() || right_type->is<NoneType>())
        {
            return pool->get_boolean_type();
        }
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
        else if (left_type->is<EnumType_ptr>())
        {
            Doctor::get().assert(
                equal(scope, left_type, right_type),
                WaspStage::Semantics,
                "Enum mismatch"
            );
        }
        else
        {
            Doctor::get().fatal(
                WaspStage::Semantics,
                "Cannot compare '" + stringify_object(left_type) + "' with '" +
                    stringify_object(right_type) + "'"
            );
        }
        return pool->get_boolean_type();

    case TokenType::AND:
    case TokenType::OR:
        expect_boolean_type(left_type);
        expect_boolean_type(right_type);
        return pool->get_boolean_type();

    default:
        Doctor::get().fatal(WaspStage::Semantics, "Unsupported binary operator");
    }
    return pool->get_none_type();
}

Object_ptr TypeSystem::infer(
    SymbolScope_ptr scope,
    Object_ptr operand_type,
    TokenType op
)
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
        return is_int_type(operand_type) ? pool->get_native_int_type()
                                         : pool->get_native_float_type();
    case TokenType::NOT:
        expect_boolean_type(operand_type);
        return pool->get_boolean_type();
    default:
        Doctor::get().fatal(WaspStage::Semantics, "Unknown unary operator");
    }
    return pool->get_none_type();
}

} // namespace Wasp
