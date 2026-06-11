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
        auto block = parse_block(loop_indent_level + 1);
        return make_statement(SimpleLoop(condition, loop_style, block));
    }

    auto statement = parse_expression_statement();
    auto block = Block{{statement}};
    return make_statement(SimpleLoop(condition, loop_style, block));
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
    Expression_ptr iterable = infix.right;

    // 4. Require the loop body
    token_pipe.require_in_line(TokenType::DO);

    if (token_pipe.consume_optional_in_line(TokenType::EOL))
    {
        auto block = parse_block(loop_indent_level + 1);
        return make_statement(ForInLoop{is_mutable, lhs, iterable, block});
    }

    auto statement = parse_expression_statement();
    Doctor::get().fatal_if_nullptr(statement, WaspStage::Parser);

    Block block = Block{{statement}};
    return make_statement(ForInLoop{is_mutable, lhs, iterable, block});
}

Statement_ptr Parser::parse_loop_control_statement(TokenType control_type)
{
    token_pipe.advance_pointer();

    token_pipe.require_in_line(TokenType::EOL);
    return make_statement(LoopControl{control_type});
}

} // namespace Wasp
