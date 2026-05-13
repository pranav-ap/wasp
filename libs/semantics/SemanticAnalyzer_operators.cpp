#include "AST.h"
#include "Doctor.h"
#include "Expression.h"
#include "Objects.h"
#include "SemanticAnalyzer.h"
#include "Statement.h"
#include "SymbolScope.h"
#include "Token.h"
#include "Workspace.h"

#include <cctype>
#include <ctime>
#include <memory>
#include <string>
#include <vector>

namespace Wasp
{

Object_ptr SemanticAnalyzer::resolve_operator_overload(
    OperatorExpression& expr,
    const std::string& operator_name,
    const ObjectVector& operand_types
)
{
    Symbol_ptr operator_symbol = current_scope->lookup(operator_name);

    Doctor::get().fatal_if_nullptr(
        operator_symbol,
        WaspStage::Semantics,
        "Undefined operator '" + operator_name + "'."
    );

    operator_symbol = operator_symbol->resolve();

    Doctor::get().assert(
        operator_symbol->payload_is<OverloadsData>(),
        WaspStage::Semantics,
        "Operator '" + operator_name +
            "' is not overloaded but used as an operator."
    );

    auto overloads = operator_symbol->get_payload_as<OverloadsData>()
                         .get_overloads();

    auto [function_symbol, overload_index] = type_system
                                                 ->get_best_function_symbol(
                                                     current_scope,
                                                     overloads,
                                                     operand_types
                                                 );

    expr.symbol = operator_symbol;
    expr.overload_index = overload_index;

    return function_symbol->get_type()->as<Signature_ptr>()->return_type;
}

Object_ptr SemanticAnalyzer::evaluate_operator(
    OperatorExpression& expr,
    TokenType fixity,
    TokenType op_type,
    const ObjectVector& operand_types
)
{
    for (const auto& type : operand_types)
    {
        if (type->is_any_of<ClassType_ptr, TraitType_ptr>())
        {
            return resolve_operator_overload(
                expr,
                get_operator_name(fixity, op_type),
                operand_types
            );
        }
    }

    if (operand_types.size() == 1)
    {
        return type_system->infer(current_scope, operand_types[0], op_type);
    }

    return type_system
        ->infer(current_scope, operand_types[0], op_type, operand_types[1]);
}

Object_ptr SemanticAnalyzer::visit(Prefix& expr)
{
    return evaluate_operator(
        expr,
        TokenType::PREFIX,
        expr.op.type,
        {visit(expr.operand)}
    );
}

Object_ptr SemanticAnalyzer::visit(Infix& expr)
{
    return evaluate_operator(
        expr,
        TokenType::INFIX,
        expr.op.type,
        {visit(expr.left), visit(expr.right)}
    );
}

Object_ptr SemanticAnalyzer::visit(Postfix& expr)
{
    return evaluate_operator(
        expr,
        TokenType::POSTFIX,
        expr.op.type,
        {visit(expr.operand)}
    );
}

} // namespace Wasp
