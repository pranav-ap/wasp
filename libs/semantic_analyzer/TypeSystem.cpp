#include "TypeSystem.h"
#include <memory>
#include <algorithm>
#include <stdexcept>

#define MAKE_OBJECT_VARIANT(x) std::make_shared<Object>(x)
#define FATAL(message) throw std::runtime_error(message)

template <class... Ts>
struct overloaded : Ts...
{
	using Ts::operator()...;
};
template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

using std::get_if;
using std::holds_alternative;
using std::vector;

namespace Wasp
{

	// ============================================================================
	// Equal Checks
	// ============================================================================

	bool TypeSystem::equal(SymbolScope_ptr scope, const Object_ptr type_1, const Object_ptr type_2) const
	{
		if (!type_1 || !type_2)
			return false;
		if (type_1 == type_2)
			return true; // Quick memory check

		// Extract Variant logic outside to prevent template explosion
		if (type_1->is<VariantType>() && type_2->is<VariantType>())
		{
			return equal_unordered(scope, type_1->as<VariantType>().types, type_2->as<VariantType>().types);
		}

		return std::visit(overloaded{// Standard Types
									 [&](AnyType const &, AnyType const &) -> bool
									 { return true; },
									 [&](IntType const &, IntType const &) -> bool
									 { return true; },
									 [&](FloatType const &, FloatType const &) -> bool
									 { return true; },
									 [&](BooleanType const &, BooleanType const &) -> bool
									 { return true; },
									 [&](StringType const &, StringType const &) -> bool
									 { return true; },
									 [&](NoneType const &, NoneType const &) -> bool
									 { return true; },

									 // Literal Types (Must have exact same value)
									 [&](IntLiteralType const &t1, IntLiteralType const &t2) -> bool
									 { return t1.value == t2.value; },
									 [&](FloatLiteralType const &t1, FloatLiteralType const &t2) -> bool
									 { return t1.value == t2.value; },
									 [&](BooleanLiteralType const &t1, BooleanLiteralType const &t2) -> bool
									 { return t1.value == t2.value; },
									 [&](StringLiteralType const &t1, StringLiteralType const &t2) -> bool
									 { return t1.value == t2.value; },

									 // Composites
									 [&](ListType const &t1, ListType const &t2) -> bool
									 { return equal(scope, t1.element_type, t2.element_type); },
									 [&](SetType const &t1, SetType const &t2) -> bool
									 { return equal(scope, t1.element_type, t2.element_type); },
									 [&](TupleType const &t1, TupleType const &t2) -> bool
									 { return equal(scope, t1.element_types, t2.element_types); },
									 [&](MapType const &t1, MapType const &t2) -> bool
									 {
										 return equal(scope, t1.key_type, t2.key_type) && equal(scope, t1.value_type, t2.value_type);
									 },

									 [](const auto &, const auto &) -> bool
									 { return false; }},
						  type_1->value, type_2->value);
	}

	bool TypeSystem::equal(SymbolScope_ptr scope, const ObjectVector type_vector_1, const ObjectVector type_vector_2) const
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

	bool TypeSystem::equal_unordered(SymbolScope_ptr scope, const ObjectVector left_vector, const ObjectVector right_vector) const
	{
		if (left_vector.size() != right_vector.size())
			return false;
		for (auto left : left_vector)
		{
			bool found = std::any_of(right_vector.begin(), right_vector.end(),
									 [&](auto right)
									 { return equal(scope, left, right); });
			if (!found)
				return false;
		}
		return true;
	}

	// ============================================================================
	// Assignable Checks (LHS <- RHS)
	// ============================================================================
	bool TypeSystem::assignable(SymbolScope_ptr scope, const Object_ptr lhs_type, const Object_ptr rhs_type) const
	{
		if (!lhs_type || !rhs_type)
			return false;
		if (equal(scope, lhs_type, rhs_type))
			return true;

		// if RHS is a variant, ALL its elements must fit LHS
		if (rhs_type->is<VariantType>())
		{
			auto &rhs_var = rhs_type->as<VariantType>();
			return std::all_of(rhs_var.types.begin(), rhs_var.types.end(),
							   [&](Object_ptr t)
							   { return assignable(scope, lhs_type, t); });
		}

		// if LHS is a variant, RHS must fit into AT LEAST ONE element of LHS
		if (lhs_type->is<VariantType>())
		{
			auto &lhs_var = lhs_type->as<VariantType>();
			return std::any_of(lhs_var.types.begin(), lhs_var.types.end(),
							   [&](Object_ptr t)
							   { return assignable(scope, t, rhs_type); });
		}

		return std::visit(overloaded{[](AnyType const &, const auto &) -> bool
									 { return true; },

									 [](IntType const &, IntType const &) -> bool
									 { return true; },
									 [](FloatType const &, FloatType const &) -> bool
									 { return true; },
									 [](BooleanType const &, BooleanType const &) -> bool
									 { return true; },
									 [](StringType const &, StringType const &) -> bool
									 { return true; },

									 // Standard types accept their literal counterparts
									 [](IntType const &, IntLiteralType const &) -> bool
									 { return true; },
									 [](FloatType const &, FloatLiteralType const &) -> bool
									 { return true; },
									 [](BooleanType const &, BooleanLiteralType const &) -> bool
									 { return true; },
									 [](StringType const &, StringLiteralType const &) -> bool
									 { return true; },

									 // Handle nested composite types recursively
									 [&](ListType const &t1, ListType const &t2) -> bool
									 {
										 return assignable(scope, t1.element_type, t2.element_type);
									 },
									 [&](SetType const &t1, SetType const &t2) -> bool
									 {
										 return assignable(scope, t1.element_type, t2.element_type);
									 },
									 [&](TupleType const &t1, TupleType const &t2) -> bool
									 {
										 return assignable(scope, t1.element_types, t2.element_types);
									 },
									 [&](MapType const &t1, MapType const &t2) -> bool
									 {
										 return assignable(scope, t1.key_type, t2.key_type) &&
												assignable(scope, t1.value_type, t2.value_type);
									 },

									 // Catch-all: Types do not match
									 [](const auto &, const auto &) -> bool
									 { return false; }

						  },
						  lhs_type->value, rhs_type->value);
	}

	bool TypeSystem::assignable(SymbolScope_ptr scope, const ObjectVector type_vector_1, const ObjectVector type_vector_2) const
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

	Object_ptr TypeSystem::infer(SymbolScope_ptr scope, Object_ptr left_type, TokenType op, Object_ptr right_type)
	{
		switch (op)
		{
		case TokenType::PLUS:
			// String Concatenation
			if (is_string_type(left_type))
			{
				if (!is_string_type(right_type) && !is_number_type(right_type))
				{
					FATAL("Semantic Error: Cannot concatenate string with this type.");
				}
				return type_pool->get_string_type();
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
				expect_number_type(right_type);
			else if (is_string_type(left_type))
				expect_string_type(right_type);
			else if (is_boolean_type(left_type))
				expect_boolean_type(right_type);
			else
				FATAL("Semantic Error: Invalid types for equality comparison.");

			return type_pool->get_boolean_type();
		}
		case TokenType::AND:
		case TokenType::OR:
		{
			expect_boolean_type(left_type);
			expect_boolean_type(right_type);
			return type_pool->get_boolean_type();
		}
		default:
			FATAL("Semantic Error: Unknown binary operator.");
		}
		return type_pool->get_none_type();
	}

	Object_ptr TypeSystem::infer(SymbolScope_ptr scope, Object_ptr operand_type, TokenType op)
	{
		switch (op)
		{
		case TokenType::PLUS:
		case TokenType::MINUS:
			expect_number_type(operand_type);
			return is_int_type(operand_type) ? type_pool->get_int_type() : type_pool->get_float_type();
		case TokenType::NOT:
			expect_boolean_type(operand_type);
			return type_pool->get_boolean_type();
		default:
			FATAL("Semantic Error: Unknown unary operator.");
		}
		return type_pool->get_none_type();
	}

	// ============================================================================
	// Spread Type
	// ============================================================================

	Object_ptr TypeSystem::spread_type(Object_ptr type)
	{
		if (!type)
			return nullptr;

		return std::visit(overloaded{[&](ListType const &t)
									 { return t.element_type; },
									 [&](TupleType const &t)
									 { return MAKE_OBJECT_VARIANT(VariantType(t.element_types)); },
									 [&](MapType const &t)
									 {
										 // Spreading a map returns elements that are Tuples of (Key, Value)
										 ObjectVector kv_types = {t.key_type, t.value_type};
										 return MAKE_OBJECT_VARIANT(TupleType(kv_types));
									 },
									 [&](const auto &)
									 {
										 FATAL("Semantic Error: Cannot spread a non-iterable type.");
										 return type_pool->get_none_type();
									 }},
						  type->value);
	}

	// ============================================================================
	// Type Checks
	// ============================================================================

	bool TypeSystem::is_boolean_type(const Object_ptr type) const
	{
		return type && (holds_alternative<BooleanType>(type->value) || holds_alternative<BooleanLiteralType>(type->value));
	}

	bool TypeSystem::is_int_type(const Object_ptr type) const
	{
		return type && (holds_alternative<IntType>(type->value) || holds_alternative<IntLiteralType>(type->value));
	}

	bool TypeSystem::is_float_type(const Object_ptr type) const
	{
		return type && (holds_alternative<FloatType>(type->value) || holds_alternative<FloatLiteralType>(type->value));
	}

	bool TypeSystem::is_string_type(const Object_ptr type) const
	{
		return type && (holds_alternative<StringType>(type->value) || holds_alternative<StringLiteralType>(type->value));
	}

	bool TypeSystem::is_none_type(const Object_ptr type) const
	{
		return type && holds_alternative<NoneType>(type->value);
	}

	bool TypeSystem::is_number_type(const Object_ptr type) const
	{
		return is_int_type(type) || is_float_type(type);
	}

	bool TypeSystem::is_condition_type(SymbolScope_ptr scope, const Object_ptr condition_type) const
	{
		if (!condition_type)
			return false;

		return std::visit(overloaded{[](BooleanType const &)
									 { return true; },
									 [](BooleanLiteralType const &)
									 { return true; },

									 // Truthiness evaluated at runtime based on length
									 [](StringType const &)
									 { return true; },
									 [](StringLiteralType const &)
									 { return true; },

									 // Truthiness evaluated at runtime based on size
									 [](ListType const &)
									 { return true; },
									 [](TupleType const &)
									 { return true; },
									 [](SetType const &)
									 { return true; },
									 [](MapType const &)
									 { return true; },

									 // If it's a Variant (e.g., `string | bool`), all possible types must be valid conditions
									 [&](VariantType const &type)
									 {
										 return std::all_of(type.types.begin(), type.types.end(), [&](Object_ptr t)
															{ return is_condition_type(scope, t); });
									 },

									 // Anything else (Functions, NoneType, etc.) is rejected
									 [](const auto &)
									 { return false; }},
						  condition_type->value);
	}

	bool TypeSystem::is_spreadable_type(SymbolScope_ptr scope, const Object_ptr candidate_type) const
	{
		if (!candidate_type)
			return false;
		return std::visit(overloaded{[&](ListType const &)
									 { return true; },
									 [&](TupleType const &)
									 { return true; },
									 [&](MapType const &)
									 { return true; },
									 [&](VariantType const &type)
									 {
										 return std::all_of(type.types.begin(), type.types.end(), [&](Object_ptr t)
															{ return is_spreadable_type(scope, t); });
									 },
									 [](const auto &)
									 { return false; }},
						  candidate_type->value);
	}

	bool TypeSystem::is_iterable_type(SymbolScope_ptr scope, const Object_ptr candidate_type) const
	{
		return is_string_type(candidate_type) || is_spreadable_type(scope, candidate_type);
	}

	bool TypeSystem::is_key_type(SymbolScope_ptr scope, const Object_ptr key_type) const
	{
		return is_int_type(key_type) || is_float_type(key_type) || is_string_type(key_type) || is_boolean_type(key_type);
	}

	// ============================================================================
	// Assert Type
	// ============================================================================

	void TypeSystem::expect_boolean_type(const Object_ptr type) const
	{
		if (!is_boolean_type(type))
			FATAL("Semantic Error: Expected a boolean type.");
	}

	void TypeSystem::expect_number_type(const Object_ptr type) const
	{
		if (!is_number_type(type))
			FATAL("Semantic Error: Expected a number type.");
	}

	void TypeSystem::expect_int_type(const Object_ptr type) const
	{
		if (!is_int_type(type))
			FATAL("Semantic Error: Expected an integer type.");
	}

	void TypeSystem::expect_float_type(const Object_ptr type) const
	{
		if (!is_float_type(type))
			FATAL("Semantic Error: Expected a float type.");
	}

	void TypeSystem::expect_string_type(const Object_ptr type) const
	{
		if (!is_string_type(type))
			FATAL("Semantic Error: Expected a string type.");
	}

	void TypeSystem::expect_none_type(const Object_ptr type) const
	{
		if (!is_none_type(type))
			FATAL("Semantic Error: Expected None type.");
	}

	void TypeSystem::expect_condition_type(SymbolScope_ptr scope, const Object_ptr type) const
	{
		if (!is_condition_type(scope, type))
			FATAL("Semantic Error: Expected a valid condition type (boolean).");
	}

	void TypeSystem::expect_spreadable_type(SymbolScope_ptr scope, const Object_ptr type) const
	{
		if (!is_spreadable_type(scope, type))
			FATAL("Semantic Error: Expected a spreadable collection type.");
	}

	void TypeSystem::expect_iterable_type(SymbolScope_ptr scope, const Object_ptr type) const
	{
		if (!is_iterable_type(scope, type))
			FATAL("Semantic Error: Expected an iterable type.");
	}

	void TypeSystem::expect_key_type(SymbolScope_ptr scope, const Object_ptr type) const
	{
		if (!is_key_type(scope, type))
			FATAL("Semantic Error: Type cannot be used as a Dictionary/Map key.");
	}

	// ============================================================================
	// Type Utilities
	// ============================================================================

	bool TypeSystem::any_eq(SymbolScope_ptr scope, const ObjectVector vec, const Object_ptr x) const
	{
		for (const auto &item : vec)
		{
			if (equal(scope, item, x))
			{
				return true;
			}
		}
		return false;
	}

	ObjectVector TypeSystem::remove_duplicates(SymbolScope_ptr scope, const ObjectVector vec) const
	{
		ObjectVector unique_types;
		unique_types.reserve(vec.size());

		for (const auto &item : vec)
		{
			if (!any_eq(scope, unique_types, item))
			{
				unique_types.push_back(item);
			}
		}

		return unique_types;
	}
}