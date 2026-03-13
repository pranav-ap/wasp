#include "Doctor.h"
#include "Expression.h"
#include "Parser.h"
#include "Token.h"

#include <map>
#include <memory>
#include <utility>

namespace Wasp {

Expression_ptr Parser::parse_expression() { return parse_expression(0); }

Expression_ptr Parser::parse_expression(const int precedence) {
    auto token = token_pipe.current_in_line();

    // Backtracking is a valid state here, so we use a native 'if'
    if (!token)
        return nullptr;

    auto token_type = token->type;

    if (token_type == TokenType::LET) {
        token_pipe.advance_pointer();
        Expression_ptr expr = parse_expression();
        // Replaced macro with direct, zero-copy instantiation
        return std::make_shared<Expression>(VariableDefinitionExpression(expr, true));
    }

    if (token_type == TokenType::CONST_KEYWORD) {
        token_pipe.advance_pointer();
        Expression_ptr expr = parse_expression();
        return std::make_shared<Expression>(VariableDefinitionExpression(expr, false));
    }

    // ------------------------------------------------------------------
    // Prefix Parsing
    // ------------------------------------------------------------------

    auto prefix_it = prefix_parselets.find(token_type);
    if (prefix_it == prefix_parselets.end()) {
        Doctor::get().fatal(
            WaspStage::Parser,
            "Unexpected token or missing prefix parselet",
            token->line,
            token->column
        );
    }

    Expression_ptr left = prefix_it->second->parse(*this, *token);
    Doctor::get().fatal_if_nullptr(left, WaspStage::Parser, token->line, token->column);

    // ------------------------------------------------------------------
    // Infix Parsing
    // ------------------------------------------------------------------

    while (precedence < get_next_operator_precedence()) {
        token = token_pipe.current_in_line();
        Doctor::get().fatal_if_nullopt(token, WaspStage::Parser);

        token_pipe.advance_pointer();

        auto infix_it = infix_parselets.find(token->type);

        // This should never realistically trigger because get_next_operator_precedence
        // already checks if it exists, but it's great internal documentation/safety.
        Doctor::get().assert(
            infix_it != infix_parselets.end(),
            WaspStage::Parser,
            "Missing infix parselet during loop",
            token->line,
            token->column
        );

        left = infix_it->second->parse(*this, left, *token);
    }

    return left;
}

ExpressionVector Parser::parse_expressions() {
    ExpressionVector elements;

    while (auto element = parse_expression()) {
        elements.push_back(std::move(element));

        // If there's a comma, consume it and continue the loop
        if (const auto token = token_pipe.later();
            token.has_value() && token->type == TokenType::COMMA) {
            token_pipe.advance_pointer();
            continue;
        }

        // No comma? We're done with the list.
        break;
    }

    return elements;
}

int Parser::get_next_operator_precedence() {
    if (const auto token = token_pipe.current_in_line()) {
        auto it = infix_parselets.find(token->type);
        if (it != infix_parselets.end()) {
            return it->second->get_precedence();
        }
    }

    return 0;
}

} // namespace Wasp