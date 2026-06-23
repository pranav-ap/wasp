#include "AST.h"
#include "Doctor.h"
#include "Expression.h"
#include "Terminator.h"
#include "Token.h"
#include "Type.h"

#include <string>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

Type_ptr Terminator::visit(Prefix& expr)
{
    auto operand_type = visit(expr.operand);

    if (type_system->is_primitive_type(operand_type))
    {
        return type_system->infer(
            current_scope,
            expr.op.type,
            type_system->unpack_primitive(operand_type)
        );
    }

    std::string function_name = get_operator_name(
        TokenType::PREFIX,
        expr.op.type
    );

    auto function_id = make_expression(Identifier(function_name));

    auto function_call = make_expression(
        Call(function_id, {}, {expr.operand})
    );

    auto resolved_type = visit(function_call);

    Doctor::semantics().fatal_if_nullptr(
        resolved_type,
        "Could not resolve operator overload : " + function_name
    );

    return resolved_type;
}

Type_ptr Terminator::visit(Infix& expr)
{
    Type_ptr left_value = visit(expr.left);
    Type_ptr right_value = visit(expr.right);

    if (type_system->is_primitive_type(left_value) &&
        type_system->is_primitive_type(right_value))
    {
        return type_system->infer(
            current_scope,
            type_system->unpack_primitive(left_value),
            expr.op.type,
            type_system->unpack_primitive(right_value)
        );
    }

    // create func call

    std::string function_name = get_operator_name(
        TokenType::INFIX,
        expr.op.type
    );

    auto function_id = make_expression(Identifier(function_name));

    auto function_call = make_expression(
        Call(function_id, {}, {expr.left, expr.right})
    );

    auto resolved_type = visit(function_call);

    Doctor::semantics().fatal_if_nullptr(
        resolved_type,
        "Could not resolve operator overload : " + function_name
    );

    return resolved_type;
}
} // namespace Wasp
