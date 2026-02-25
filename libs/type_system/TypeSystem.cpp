#include "TypeSystem.h"
#include <memory>
#include <algorithm>
#include <stdexcept>

#ifndef ASSERT
#include <cassert>
#define ASSERT(condition, message) assert((condition) && message)
#endif

#define MAKE_OBJECT_VARIANT(x) std::make_shared<Object>(x)
#define FATAL(message) throw std::runtime_error(message)
#define VOID std::make_shared<Object>(std::make_shared<ReturnObject>())
#define NULL_CHECK(x) ASSERT(x != nullptr, "Oh shit! A nullptr")
#define OPT_CHECK(x) ASSERT(x.has_value(), "Oh shit! Option is none")
#define THROW(message) return std::make_shared<Object>(std::make_shared<ErrorObject>(message));
#define THROW_IF(condition, message) if (!(condition)) { return std::make_shared<Object>(std::make_shared<ErrorObject>(message)); }

template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...)->overloaded<Ts...>;

using std::holds_alternative;
using std::get_if;
using std::vector;
using std::make_shared;
using std::all_of;

namespace Wasp { 
    
// ============================================================================
// Equal Checks
// ============================================================================

bool TypeSystem::equal(SymbolScope_ptr scope, const Object_ptr type_1, const Object_ptr type_2) const
{
    if (!type_1 || !type_2) return false;

    return std::visit(overloaded{
        [&](AnyType const&, AnyType const&) { return true; },
        [&](IntType const&, IntType const&) { return true; },
        [&](FloatType const&, FloatType const&) { return true; },
        [&](BooleanType const&, BooleanType const&) { return true; },
        [&](StringType const&, StringType const&) { return true; },

        [&](IntLiteralType const& t1, IntLiteralType const& t2) { return t1.value == t2.value; },
        [&](FloatLiteralType const& t1, FloatLiteralType const& t2) { return t1.value == t2.value; },
        [&](BooleanLiteralType const& t1, BooleanLiteralType const& t2) { return t1.value == t2.value; },
        [&](StringLiteralType const& t1, StringLiteralType const& t2) { return t1.value == t2.value; },

        [&](ListType const& t1, ListType const& t2) { return equal(scope, t1.element_type, t2.element_type); },
        [&](SetType const& t1, SetType const& t2) { return equal(scope, t1.element_type, t2.element_type); },
        [&](TupleType const& t1, TupleType const& t2) { return equal(scope, t1.element_types, t2.element_types); },
        [&](VariantType const& t1, VariantType const& t2) { return equal_unordered(scope, t1.types, t2.types); },

        [&](MapType const& t1, MapType const& t2) {
            return equal(scope, t1.key_type, t2.key_type) && equal(scope, t1.value_type, t2.value_type);
        },

        [&](NoneType const&, NoneType const&) { return true; },

        [](const auto&, const auto&) { return false; }
    }, type_1->value, type_2->value); 
}


bool TypeSystem::equal(SymbolScope_ptr scope, const ObjectVector type_vector_1, const ObjectVector type_vector_2) const
{
	int type_vector_1_length = type_vector_1.size();
	int type_vector_2_length = type_vector_2.size();

	if (type_vector_1_length != type_vector_2_length)
	{
		return false;
	}

	for (int index = 0; index < type_vector_1_length; index++)
	{
		auto type_1 = type_vector_1.at(index);
		auto type_2 = type_vector_2.at(index);

		if (!equal(scope, type_1, type_2))
		{
			return false;
		}
	}

	return true;
}

bool TypeSystem::equal_unordered(SymbolScope_ptr scope, const ObjectVector left_vector, const ObjectVector right_vector) const
{
	int type_vector_1_length = left_vector.size();
	int type_vector_2_length = right_vector.size();

	if (type_vector_1_length != type_vector_2_length)
	{
		return false;
	}

	for (auto left : left_vector)
	{
		bool any_equal = std::any_of(begin(right_vector), end(right_vector),
			[&](auto right)
			{
				return equal(scope, left, right);
			}
		);

		if (!any_equal)
		{
			return false;
		}
	}

	return true;
}

// ============================================================================
// Assignable Checks
// ============================================================================

bool TypeSystem::assignable(SymbolScope_ptr scope, const Object_ptr lhs_type, const Object_ptr rhs_type) const
{
    if (!lhs_type || !rhs_type) return false;

    if (equal(scope, lhs_type, rhs_type)) return true;

    return std::visit(overloaded{
        [&](AnyType const&, const auto&) { return true; },

        [&](IntType const&, IntLiteralType const&) { return true; },
        [&](FloatType const&, FloatLiteralType const&) { return true; },
        [&](BooleanType const&, BooleanLiteralType const&) { return true; },
        [&](StringType const&, StringLiteralType const&) { return true; },

        [&](IntLiteralType const&, IntType const&) { return true; },
        [&](FloatLiteralType const&, FloatType const&) { return true; },
        [&](BooleanLiteralType const&, BooleanType const&) { return true; },
        [&](StringLiteralType const&, StringType const&) { return true; },

        [&](ListType const& t1, ListType const& t2) { return assignable(scope, t1.element_type, t2.element_type); },
        [&](SetType const& t1, SetType const& t2) { return assignable(scope, t1.element_type, t2.element_type); },
        [&](TupleType const& t1, TupleType const& t2) { return assignable(scope, t1.element_types, t2.element_types); },

        [&](MapType const& t1, MapType const& t2) {
            return assignable(scope, t1.key_type, t2.key_type) && assignable(scope, t1.value_type, t2.value_type);
        },

        [&](VariantType const& lhs_variant, const auto&) {
            return std::all_of(lhs_variant.types.begin(), lhs_variant.types.end(), 
                               [&](Object_ptr t) { return assignable(scope, t, rhs_type); });
        },

        [](const auto&, const auto&) { return false; }
    }, lhs_type->value, rhs_type->value);
}


bool TypeSystem::assignable(SymbolScope_ptr scope, const ObjectVector type_vector_1, const ObjectVector type_vector_2) const
{
	int type_vector_1_length = type_vector_1.size();
	int type_vector_2_length = type_vector_2.size();

	if (type_vector_1_length != type_vector_2_length)
	{
		return false;
	}

	for (int index = 0; index < type_vector_1_length; index++)
	{
		auto type_1 = type_vector_1.at(index);
		auto type_2 = type_vector_2.at(index);

		if (!assignable(scope, type_1, type_2))
		{
			return false;
		}
	}

	return true;
}

// INFERENCE


Object_ptr TypeSystem::infer(SymbolScope_ptr scope, Object_ptr left_type, TokenType op, Object_ptr right_type)
{
	switch (op)
	{
	case TokenType::PLUS:
	case TokenType::STAR:
	case TokenType::POWER:
	{
		if (is_number_type(left_type))
		{
			expect_number_type(right_type);
			return is_int_type(right_type) ? type_pool->get_int_type() : type_pool->get_float_type();
		}
		else if (is_string_type(left_type))
		{
			ASSERT(is_number_type(right_type) || is_string_type(right_type), "Number or string operand is expected");
			return type_pool->get_string_type();
		}

		FATAL("Number or string operand is expected");
		break;
	}
	case TokenType::MINUS:
	case TokenType::DIVISION:
	case TokenType::REMINDER:
	{
		expect_number_type(left_type);
		expect_number_type(right_type);

		if (is_float_type(left_type) || is_float_type(right_type))
		{
			return type_pool->get_float_type();
		}

		return type_pool->get_int_type();
	}
	case TokenType::LESSER_THAN:
	case TokenType::LESSER_THAN_EQUAL:
	case TokenType::GREATER_THAN:
	case TokenType::GREATER_THAN_EQUAL:
	{
		expect_number_type(left_type);
		expect_number_type(right_type);
		return type_pool->get_boolean_type();
	}
	case TokenType::EQUAL_EQUAL:
	case TokenType::BANG_EQUAL:
	{
		if (is_number_type(left_type))
		{
			expect_number_type(right_type);
			return type_pool->get_boolean_type();
		}
		else if (is_string_type(left_type))
		{
			expect_string_type(right_type);
			return type_pool->get_boolean_type();
		}
		else if (is_boolean_type(left_type))
		{
			expect_boolean_type(right_type);
			return type_pool->get_boolean_type();
		}

		FATAL("Number or string or boolean operand is expected");
		break;
	}
	case TokenType::AND:
	case TokenType::OR:
	{
		expect_boolean_type(left_type);
		expect_boolean_type(right_type);
		return type_pool->get_boolean_type();
	}
	default:
	{
		FATAL("What the hell is this Binary statement?");
		break;
	}
	}

	return type_pool->get_none_type();
}

Object_ptr TypeSystem::infer(SymbolScope_ptr scope, Object_ptr operand_type, TokenType op)
{
	switch (op)
	{
	case TokenType::PLUS:
	case TokenType::MINUS:
	{
		expect_number_type(operand_type);
		return is_int_type(operand_type) ? type_pool->get_int_type() : type_pool->get_float_type();
	}
	case TokenType::NOT:
	{
		expect_boolean_type(operand_type);
		return type_pool->get_boolean_type();
	}
	case TokenType::TYPE_OF:
	{
		return operand_type;
	}
	default:
	{
		FATAL("What the hell is this unary statement?");
		break;
	}
	}

	return type_pool->get_none_type();
}


// ============================================================================
// Spread Type
// ============================================================================

Object_ptr TypeSystem::spread_type(Object_ptr type)
{
    if (!type) return nullptr;

    return std::visit(overloaded{
        [&](ListType const& t) { return t.element_type; },
        [&](TupleType const& t) { return MAKE_OBJECT_VARIANT(VariantType(t.element_types)); },
        [&](MapType const& t) { return type; },
        [&](const auto&) { 
            FATAL("Cannot spread this type");
            return type_pool->get_none_type(); 
        }
    }, type->value); 
}

// ============================================================================
// Type Identifiers (Is_X)
// ============================================================================

// Target type->value for all variants
bool TypeSystem::is_boolean_type(const Object_ptr type) const { return type && holds_alternative<BooleanType>(type->value); }
bool TypeSystem::is_int_type(const Object_ptr type) const { return type && holds_alternative<IntType>(type->value); }
bool TypeSystem::is_float_type(const Object_ptr type) const { return type && holds_alternative<FloatType>(type->value); }
bool TypeSystem::is_string_type(const Object_ptr type) const { return type && holds_alternative<StringType>(type->value); }
bool TypeSystem::is_none_type(const Object_ptr type) const { return type && holds_alternative<NoneType>(type->value); }

bool TypeSystem::is_number_type(const Object_ptr type) const
{
    return type && (holds_alternative<IntType>(type->value) || holds_alternative<FloatType>(type->value));
}

bool TypeSystem::is_condition_type(SymbolScope_ptr scope, const Object_ptr condition_type) const
{
    if (!condition_type) return false;
    return std::visit(overloaded{
        [&](VariantType const& type) {
            return std::all_of(type.types.begin(), type.types.end(), [&](Object_ptr t) { return is_condition_type(scope, t); });
        },
        [](const auto&) { return true; }
    }, condition_type->value);
}

bool TypeSystem::is_spreadable_type(SymbolScope_ptr scope, const Object_ptr candidate_type) const
{
    if (!candidate_type) return false;
    return std::visit(overloaded{
        [&](ListType const&) { return true; },
        [&](TupleType const&) { return true; },
        [&](MapType const&) { return true; },
        [&](VariantType const& type) {
            return std::all_of(type.types.begin(), type.types.end(), [&](Object_ptr t) { return is_spreadable_type(scope, t); });
        },
        [](const auto&) { return false; }
    }, candidate_type->value);
}

bool TypeSystem::is_iterable_type(SymbolScope_ptr scope, const Object_ptr candidate_type) const
{
    if (!candidate_type) return false;
    return std::visit(overloaded{
        [&](StringType const&) { return true; },
        [&](ListType const&) { return true; },
        [&](MapType const&) { return true; },
        [&](VariantType const& type) {
            return std::all_of(type.types.begin(), type.types.end(), [&](Object_ptr t) { return is_iterable_type(scope, t); });
        },
        [](const auto&) { return false; }
    }, candidate_type->value);
}

bool TypeSystem::is_key_type(SymbolScope_ptr scope, const Object_ptr key_type) const
{
    if (!key_type) return false;
    return std::visit(overloaded{
        [&](IntType const&) { return true; },
        [&](FloatType const&) { return true; },
        [&](BooleanType const&) { return true; },
        [&](StringType const&) { return true; },
        [&](IntLiteralType const&) { return true; },
        [&](FloatLiteralType const&) { return true; },
        [&](StringLiteralType const&) { return true; },
        [&](BooleanLiteralType const&) { return true; },
        [&](VariantType const& type) {
            return std::all_of(type.types.begin(), type.types.end(), [&](Object_ptr t) { return is_key_type(scope, t); });
        },
        [](const auto&) { return false; }
    }, key_type->value);
}

// assert type

void TypeSystem::expect_boolean_type(const Object_ptr type) const
{
	ASSERT(is_boolean_type(type), "Must be a BooleanType");
}

void TypeSystem::expect_number_type(const Object_ptr type) const
{
	ASSERT(is_number_type(type), "Must be a Number Type");
}

void TypeSystem::expect_int_type(const Object_ptr type) const
{
	ASSERT(is_int_type(type), "Must be a IntType");
}

void TypeSystem::expect_float_type(const Object_ptr type) const
{
	ASSERT(is_float_type(type), "Must be a FloatType");
}

void TypeSystem::expect_string_type(const Object_ptr type) const
{
	ASSERT(is_string_type(type), "Must be a StringType");
}

void TypeSystem::expect_none_type(const Object_ptr type) const
{
	ASSERT(is_none_type(type), "Must be a NoneType");
}

void TypeSystem::expect_condition_type(SymbolScope_ptr scope, const Object_ptr type) const
{
	ASSERT(is_condition_type(scope, type), "Must be a Condition Type");
}

void TypeSystem::expect_spreadable_type(SymbolScope_ptr scope, const Object_ptr type) const
{
	ASSERT(is_spreadable_type(scope, type), "Must be a Spreadable type");
}

void TypeSystem::expect_iterable_type(SymbolScope_ptr scope, const Object_ptr type) const
{
	ASSERT(is_iterable_type(scope, type), "Must be a iterable Type");
}

void TypeSystem::expect_key_type(SymbolScope_ptr scope, const Object_ptr type) const
{
	ASSERT(is_key_type(scope, type), "Must be a key Type");
}

}