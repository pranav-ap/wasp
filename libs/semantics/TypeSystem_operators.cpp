#include "Doctor.h"
#include "SymbolScope.h"
#include "Token.h"
#include "Type.h"
#include "TypeSystem.h"

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

Type_ptr TypeSystem::infer(
    SymbolScope_ptr scope,
    const Type_ptr left,
    const TokenType op,
    const Type_ptr right
) const
{
    Type_ptr left_type = left->unwrap_alias();
    Type_ptr right_type = right->unwrap_alias();

    if (left_type->is<VariantType_ptr>())
    {
        TypeVector result_types;
        auto variant = left_type->as<VariantType_ptr>();

        for (const auto& t : variant->types)
        {
            result_types.push_back(infer(scope, t, op, right_type));
        }

        return make_shared_type<VariantType>(result_types);
    }

    if (right_type->is<VariantType_ptr>())
    {
        TypeVector result_types;
        auto variant = right_type->as<VariantType_ptr>();

        for (const auto& t : variant->types)
        {
            result_types.push_back(infer(scope, left_type, op, t));
        }

        return make_shared_type<VariantType>(result_types);
    }

    if (left_type->is<IntersectionType_ptr>())
    {
        TypeVector result_types;
        auto intersection = left_type->as<IntersectionType_ptr>();

        for (const auto& t : intersection->types)
        {
            result_types.push_back(infer(scope, t, op, right_type));
        }

        return make_shared_type<IntersectionType>(result_types);
    }

    if (right_type->is<IntersectionType_ptr>())
    {
        TypeVector result_types;
        auto intersection = right_type->as<IntersectionType_ptr>();

        for (const auto& t : intersection->types)
        {
            result_types.push_back(infer(scope, left_type, op, t));
        }

        return make_shared_type<IntersectionType>(result_types);
    }

    switch (op)
    {
    case TokenType::PLUS:
        if (is_string_type(left_type) || is_string_type(right_type))
        {
            bool left_valid = is_string_type(left_type) ||
                              is_number_type(left_type);
            bool right_valid = is_string_type(right_type) ||
                               is_number_type(right_type);
            if (!left_valid)
            {
                Doctor::semantics().fatal(
                    "Invalid concatenation: Left is '" +
                    left_type->to_string() + "'"
                );
            }
            if (!right_valid)
            {
                Doctor::semantics().fatal(
                    "Invalid concatenation: Right is '" +
                    right_type->to_string() + "'"
                );
            }
            return make_shared_type<StringType>();
        }
        [[fallthrough]];
    case TokenType::STAR:
    case TokenType::POWER:
    case TokenType::MINUS:
    case TokenType::DIVISION:
    case TokenType::MOD: {
        Doctor::semantics().assert(
            is_number_type(left_type),
            "Left operand must be a number, got '" +
                left_type->to_string() + "'"
        );

        Doctor::semantics().assert(
            is_number_type(right_type),
            "Right operand must be a number, got '" +
                right_type->to_string() + "'"
        );

        return (is_float_type(left_type) || is_float_type(right_type))
                   ? make_shared_type<FloatType>()
                   : make_shared_type<IntType>();
    }

    case TokenType::LESSER_THAN:
    case TokenType::LESSER_THAN_EQUAL:
    case TokenType::GREATER_THAN:
    case TokenType::GREATER_THAN_EQUAL: {
        Doctor::semantics().assert(
            is_number_type(left_type),
            "Left operand must be a number, got '" +
                left_type->to_string() + "'"
        );

        Doctor::semantics().assert(
            is_number_type(right_type),
            "Right operand must be a number, got '" +
                right_type->to_string() + "'"
        );

        [[fallthrough]];
    }
    case TokenType::EQUAL_EQUAL:
    case TokenType::BANG_EQUAL: {
        if (left_type->is<NoneType_ptr>() ||
            right_type->is<NoneType_ptr>())
        {
            return make_shared_type<BooleanType>();
        }
        if (is_number_type(left_type))
        {
            Doctor::semantics().assert(
                is_number_type(right_type),
                "Right operand must be a number, got '" +
                    right_type->to_string() + "'"
            );
        }
        else if (is_string_type(left_type))
        {
            Doctor::semantics().assert(
                is_string_type(right_type),
                "Right operand must be a string, got '" +
                    right_type->to_string() + "'"
            );
        }
        else if (is_boolean_type(left_type))
        {
            Doctor::semantics().assert(
                is_boolean_type(right_type),
                "Right operand must be a boolean, got '" +
                    right_type->to_string() + "'"
            );
        }
        else if (left_type->is<EnumType_ptr>())
        {
            Doctor::semantics().assert(
                equal(scope, left_type, right_type),
                "Enum mismatch"
            );
        }
        else
        {
            Doctor::semantics().fatal(
                "Cannot compare '" + left_type->to_string() + "' with '" +
                right_type->to_string() + "'"
            );
        }
        return make_shared_type<BooleanType>();
    }

    case TokenType::AND:
    case TokenType::OR: {
        Doctor::semantics().assert(
            is_boolean_type(left_type),
            "Left operand must be a boolean, got '" +
                left_type->to_string() + "'"
        );

        Doctor::semantics().assert(
            is_boolean_type(right_type),
            "Right operand must be a boolean, got '" +
                right_type->to_string() + "'"
        );

        return make_shared_type<BooleanType>();
    }

    default: {
        Doctor::semantics().fatal("Unsupported binary operator");
    }
    }

    return make_shared_type<NoneType>();
}

Type_ptr TypeSystem::infer(
    SymbolScope_ptr scope,
    const TokenType op,
    const Type_ptr operand
) const
{
    Type_ptr operand_type = operand->unwrap_alias();

    if (operand_type->is<VariantType_ptr>())
    {
        TypeVector result_types;
        auto variant = operand_type->as<VariantType_ptr>();

        for (const auto& t : variant->types)
        {
            result_types.push_back(infer(scope, op, t));
        }

        return make_shared_type<VariantType>(result_types);
    }

    if (operand_type->is<IntersectionType_ptr>())
    {
        TypeVector result_types;
        auto intersection = operand_type->as<IntersectionType_ptr>();

        for (const auto& t : intersection->types)
        {
            result_types.push_back(infer(scope, op, t));
        }

        return make_shared_type<IntersectionType>(result_types);
    }

    return make_shared_type<NoneType>();
}

} // namespace Wasp
