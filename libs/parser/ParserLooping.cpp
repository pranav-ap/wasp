#include "AST.h"
#include "Doctor.h"
#include "Expression.h"
#include "Parser.h"
#include "Statement.h"
#include "Token.h"

#include <memory>
#include <optional>
#include <string>

namespace Wasp
{

Statement_ptr Parser::parse_simple_loop(
    TokenType loop_style,
    int loop_indent_level
)
{
    token_pipe.advance_pointer();

    auto condition = parse_expression();
    Doctor::get().fatal_if_nullptr(condition, WaspStage::Parser);

    token_pipe.require_in_line(TokenType::DO);

    if (token_pipe.consume_optional_in_line(TokenType::EOL))
    {
        auto body = parse_statements_block(loop_indent_level + 1);
        return make_statement(SimpleLoop(body, condition, loop_style));
    }

    auto statement = parse_expression_statement();
    auto body = {statement};
    return make_statement(SimpleLoop(body, condition, loop_style));
}

Statement_ptr Parser::parse_for_in_loop(int loop_indent_level)
{
    // 1. Consume the 'for' keyword
    token_pipe.advance_pointer();

    // 2. Intercept mutability flags before standard expression parsing
    bool is_mutable = false;
    auto next_token = token_pipe.current_in_line();

    if (next_token && (next_token->type == TokenType::LET ||
                       next_token->type == TokenType::CONST_KEYWORD))
    {
        is_mutable = (next_token->type == TokenType::LET);
        token_pipe.advance_pointer();
    }

    // 3. Parse the 'x in y' part as an expression (will resolve to an Infix
    // node)
    auto expression = parse_expression();
    Doctor::get().fatal_if_nullptr(expression, WaspStage::Parser);

    if (!expression->is<Infix>())
    {
        auto current_token = token_pipe.current();

        Doctor::get().fatal(
            WaspStage::Parser,
            "Expected 'EXPR in ITERABLE' after for"
        );
    }

    const auto& infix = expression->as<Infix>();

    if (infix.op.type != TokenType::IN_KEYWORD)
    {
        Doctor::get().fatal(
            WaspStage::Parser,
            "Expected 'in' keyword in for loop, but got " +
                to_string(infix.op.type)
        );
    }

    Expression_ptr lhs = infix.left;
    Expression_ptr iterable_expression = infix.right;

    // 4. Require the loop body
    token_pipe.require_in_line(TokenType::DO);

    if (token_pipe.consume_optional_in_line(TokenType::EOL))
    {
        auto body = parse_statements_block(loop_indent_level + 1);
        return make_statement(
            ForInLoop(body, lhs, iterable_expression, is_mutable)
        );
    }

    auto statement = parse_expression_statement();
    Doctor::get().fatal_if_nullptr(statement, WaspStage::Parser);

    StatementVector body = {statement};
    return make_statement(
        ForInLoop(body, lhs, iterable_expression, is_mutable)
    );
}

Statement_ptr Parser::parse_loop_control_statement(TokenType control_type)
{
    token_pipe.advance_pointer();

    auto token = token_pipe.consume_optional_in_line(TokenType::IDENTIFIER);

    if (token.has_value() && token.value().type == TokenType::IDENTIFIER)
    {
        std::string label = token.value().lexeme;
        token_pipe.require_in_line(TokenType::EOL);
        return make_statement(LoopControl{control_type, label});
    }

    token_pipe.require_in_line(TokenType::EOL);
    return make_statement(LoopControl{control_type});
}
} // namespace Wasp
