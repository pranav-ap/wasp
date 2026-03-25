#include "Doctor.h"
#include "Expression.h"
#include "Parser.h"
#include "Statement.h"
#include "Token.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

using std::make_shared;
using std::move;
using std::optional;

namespace Wasp {

Statement_ptr Parser::parse_simple_loop(TokenType loop_style, int loop_indent_level) {
    token_pipe.advance_pointer();

    auto condition = parse_expression();
    Doctor::get().fatal_if_nullptr(condition, WaspStage::Parser);

    token_pipe.require_in_line(TokenType::DO);

    if (token_pipe.consume_optional_in_line(TokenType::EOL)) {
        auto body = parse_statements_block(loop_indent_level + 1);
        return make_statement(SimpleLoop(body, condition, loop_style));
    }

    auto statement = parse_expression_statement();
    auto body = {statement};
    return make_statement(SimpleLoop(body, condition, loop_style));
}

Statement_ptr Parser::parse_for_in_loop(int loop_indent_level) {
    // Consume the 'for' keyword
    token_pipe.advance_pointer();

    // Parse the 'let x in y' or 'x in y' part as an expression
    auto expression = parse_expression();
    Doctor::get().fatal_if_nullptr(expression, WaspStage::Parser);

    Expression_ptr infix_expr = expression;
    bool is_mutable = false;

    if (expression->is<VariableDefinitionExpression>()) {
        auto& var_def_expr = expression->as<VariableDefinitionExpression>();
        is_mutable = var_def_expr.is_mutable;
        infix_expr = var_def_expr.assignment;
    }

    if (!infix_expr->is<Infix>()) {
        // We can pull the line/column from the current token pipe state for better diagnostics
        auto current_token = token_pipe.current();
        int line = current_token ? current_token->line : 0;
        int col = current_token ? current_token->column : 0;

        Doctor::get().fatal(WaspStage::Parser, "Expected 'EXPR in ITERABLE' after for", line, col);
    }

    const auto& infix = infix_expr->as<Infix>();

    if (infix.op.type != TokenType::IN_KEYWORD) {
        Doctor::get().fatal(
            WaspStage::Parser,
            "Expected 'in' keyword in for loop, but got " + to_string(infix.op.type),
            infix.op.line,
            infix.op.column
        );
    }

    Expression_ptr lhs = infix.left;
    Expression_ptr iterable_expression = infix.right;

    token_pipe.require_in_line(TokenType::DO);

    if (token_pipe.consume_optional_in_line(TokenType::EOL)) {
        auto body = parse_statements_block(loop_indent_level + 1);
        return make_statement(ForInLoop(body, lhs, iterable_expression, is_mutable));
    }

    auto statement = parse_expression_statement();
    Doctor::get().fatal_if_nullptr(statement, WaspStage::Parser);

    StatementVector body = {statement};
    return make_statement(ForInLoop(body, lhs, iterable_expression, is_mutable));
}

Statement_ptr Parser::parse_loop_control_statement(TokenType control_type) {
    token_pipe.advance_pointer();

    auto token = token_pipe.consume_optional_in_line(TokenType::IDENTIFIER);

    if (token.has_value() && token.value().type == TokenType::IDENTIFIER) {
        std::string label = token.value().value;
        token_pipe.require_in_line(TokenType::EOL);
        return make_statement(LoopControl{control_type, label});
    }

    token_pipe.require_in_line(TokenType::EOL);
    return make_statement(LoopControl{control_type});
}
} // namespace Wasp
