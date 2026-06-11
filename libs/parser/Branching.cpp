#include "AST.h"
#include "Expression.h"
#include "Parser.h"
#include "Statement.h"
#include "Token.h"

#include <utility>

namespace Wasp {

Statement_ptr Parser::parse_branching(
    TokenType token_type,
    int if_keyword_indent_level
)
{
    token_pipe.advance_pointer();

    auto test = parse_expression();
    token_pipe.require_in_line(TokenType::THEN);

    // Ternary

    if (token_type == TokenType::IF &&
        !token_pipe.consume_optional_in_line(TokenType::EOL).has_value())
    {
        Expression_ptr ternary = parse_ternary_condition(test);
        token_pipe.require_in_line(TokenType::EOL);

        return make_statement(ExpressionStatement(std::move(ternary)));
    }

    // If Block

    Block block = parse_block(if_keyword_indent_level + 1);
    token_pipe.ignore_empty_lines();

    if (token_pipe
            .peek_type_at_indent(if_keyword_indent_level, TokenType::ELIF))
    {
        token_pipe.expect_n_indents(if_keyword_indent_level);
        auto alternative = parse_branching(
            TokenType::ELIF,
            if_keyword_indent_level
        );

        return make_statement(Branch(block, test, alternative));
    }

    if (token_pipe
            .peek_type_at_indent(if_keyword_indent_level, TokenType::ELSE))
    {
        // Consumes the indents
        token_pipe.expect_n_indents(if_keyword_indent_level);
        // Consumes 'else'
        token_pipe.advance_pointer();
        auto alternative = parse_else_block(if_keyword_indent_level);
        return make_statement(Branch(block, test, alternative));
    }

    return make_statement(Branch(block, test));
}

Statement_ptr Parser::parse_else_block(int if_keyword_indent_level)
{
    token_pipe.require_in_line(TokenType::EOL);

    Block else_block = parse_block(if_keyword_indent_level + 1);
    return make_statement(Branch{std::move(else_block)});
}

Expression_ptr Parser::parse_ternary_condition(Expression_ptr test)
{
    Expression_ptr then_expr = parse_expression();

    token_pipe.require_in_line(TokenType::ELSE);

    auto else_expr = parse_expression();
    return make_expression(TernaryExpression{then_expr, test, else_expr});
}

} // namespace Wasp
