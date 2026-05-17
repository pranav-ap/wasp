#include "AST.h"
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
#include <optional>
#include <string>

namespace Wasp
{

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

Object_ptr SemanticAnalyzer::resolve_operator_overload(
    OperatorExpression& expr,
    Symbol_ptr operator_symbol,
    const ObjectVector& operand_types
)
{
    auto overloads = operator_symbol->get_overloads();

    auto [function_symbol, raw_index] = type_system->get_best_function_symbol(
        current_scope,
        overloads,
        operand_types
    );

    auto signature = function_symbol->get_type()->as<Signature_ptr>();

    // 1. If it's a template, instantiate it.
    // (try_monomorphize_operator already handles setting expr.overload_index = 0
    // internally)
    auto specialized_type = try_monomorphize_operator(
        expr,
        function_symbol,
        signature,
        operand_types
    );

    if (specialized_type.has_value())
    {
        return specialized_type.value();
    }

    // ========================================================================
    // 2. It's a concrete operator! Calculate the Runtime Index
    // ========================================================================
    int runtime_index = 0;
    for (const auto& candidate : overloads)
    {
        if (candidate == function_symbol)
        {
            break; // Found our winner, stop counting
        }

        auto cand_sig = candidate->get_type()->as<Signature_ptr>();

        // Only count it if it's concrete (templates are stripped by the VM)
        if (cand_sig->expected_generic_names_order.empty())
        {
            runtime_index++;
        }
    }

    expr.symbol = operator_symbol;
    expr.overload_index = runtime_index;

    return signature->return_type;
}

std::optional<Object_ptr> SemanticAnalyzer::try_monomorphize_operator(
    OperatorExpression& expr,
    Symbol_ptr function_symbol,
    Signature_ptr signature,
    const ObjectVector& operand_types
)
{
    if (signature->expected_generic_names_order.empty())
    {
        return std::nullopt;
    }

    ObjectStringMap substitutions = type_system->infer_template_arguments(
        signature,
        operand_types
    );

    if (substitutions.size() != signature->expected_generic_names_order.size())
    {
        return std::nullopt;
    }

    ObjectVector deduced_args;
    for (const auto& name : signature->expected_generic_names_order)
    {
        deduced_args.push_back(substitutions.at(name));
    }

    std::string specialized_name = function_symbol->name + "_" +
                                   mangle_object(deduced_args);

    Symbol_ptr specialized_group_symbol = monomorphize_callable_template(
        function_symbol,
        substitutions,
        specialized_name
    );

    expr.symbol = specialized_group_symbol;
    expr.overload_index = 0;

    auto specialized_overloads = specialized_group_symbol->get_overloads();
    return unwrap_completely(
        specialized_overloads[0]->get_type()->as<Signature_ptr>()->return_type
    );
}

Object_ptr SemanticAnalyzer::try_resolve_custom_operator(
    OperatorExpression& expr,
    Symbol_ptr operator_symbol,
    const ObjectVector& operand_types
)
{
    if (!operator_symbol->payload_is<OverloadsData>())
    {
        return nullptr;
    }

    auto overloads = operator_symbol->get_overloads();

    for (const auto& candidate : overloads)
    {
        auto signature = candidate->get_type()->as<Signature_ptr>();

        bool is_assignable = type_system->assignable(
            current_scope,
            signature->parameter_types,
            operand_types
        );

        if (is_assignable)
        {
            return resolve_operator_overload(expr, operator_symbol, operand_types);
        }
    }

    return nullptr;
}

Object_ptr SemanticAnalyzer::evaluate_operator(
    OperatorExpression& expr,
    TokenType fixity,
    TokenType op_type,
    const ObjectVector& operand_types
)
{
    std::string op_name = get_operator_name(fixity, op_type);

    if (auto symbol = current_scope->lookup(op_name))
    {
        auto resolved_type = try_resolve_custom_operator(
            expr,
            symbol->resolve(),
            operand_types
        );

        if (resolved_type)
        {
            return resolved_type;
        }
    }

    if (operand_types.size() == 1)
    {
        return type_system->infer(current_scope, operand_types[0], op_type);
    }

    return type_system
        ->infer(current_scope, operand_types[0], op_type, operand_types[1]);
}

} // namespace Wasp
