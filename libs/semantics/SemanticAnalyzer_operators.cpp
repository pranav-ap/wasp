#include "AST.h"
#include "ASTFactory.h"
#include "Doctor.h"
#include "Expression.h"
#include "Objects.h"
#include "SemanticAnalyzer.h"
#include "Statement.h"
#include "Token.h"
#include "Workspace.h"

#include <cctype>
#include <ctime>
#include <string>

namespace Wasp
{

Object_ptr SemanticAnalyzer::visit(Prefix& expr)
{
    auto operand_type = visit(expr.operand);

    std::string function_name = get_operator_name(
        TokenType::PREFIX,
        expr.op.type
    );

    auto function_call = ASTFactory::create_function_call(
        function_name,
        {expr.operand}
    );

    auto resolved_type = visit(function_call);

    Doctor::get().fatal_if_nullptr(
        resolved_type,
        WaspStage::Semantics,
        "Could not resolve operator overload for operator: " + function_name
    );

    return resolved_type;
}

Object_ptr SemanticAnalyzer::visit(Infix& expr)
{
    Object_ptr left_value = visit(expr.left);
    Object_ptr right_value = visit(expr.right);

    if (type_system->is_native_type(left_value) &&
        type_system->is_native_type(right_value))
    {
        return type_system
            ->infer(current_scope, left_value, expr.op.type, right_value);
    }

    // create func call

    std::string function_name = get_operator_name(
        TokenType::INFIX,
        expr.op.type
    );

    auto function_call = ASTFactory::create_function_call(
        function_name,
        {expr.left, expr.right}
    );

    auto resolved_type = visit(function_call);

    Doctor::get().fatal_if_nullptr(
        resolved_type,
        WaspStage::Semantics,
        "Could not resolve operator overload for operator: " + function_name
    );

    return resolved_type;
}

Object_ptr SemanticAnalyzer::visit(Postfix& expr)
{
    auto operand_type = visit(expr.operand);

    std::string function_name = get_operator_name(
        TokenType::POSTFIX,
        expr.op.type
    );

    auto function_call = ASTFactory::create_function_call(
        function_name,
        {expr.operand}
    );

    auto resolved_type = visit(function_call);

    Doctor::get().fatal_if_nullptr(
        resolved_type,
        WaspStage::Semantics,
        "Could not resolve operator overload for operator: " + function_name
    );

    return resolved_type;
}

} // namespace Wasp
