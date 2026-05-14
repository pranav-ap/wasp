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

    auto condition = parse_expression();
    token_pipe.require_in_line(TokenType::THEN);

    // Ternary

    if (token_type == TokenType::IF &&
        !token_pipe.consume_optional_in_line(TokenType::EOL).has_value())
    {
        Expression_ptr ternary = parse_ternary_condition(
            TokenType::IF,
            condition
        );
        token_pipe.require_in_line(TokenType::EOL);

        return make_statement(ExpressionStatement(std::move(ternary)));
    }

    // If Block

    StatementVector body = parse_statements_block(if_keyword_indent_level + 1);
    token_pipe.ignore_empty_lines();

    if (token_pipe
            .peek_type_at_indent(if_keyword_indent_level, TokenType::ELIF))
    {
        token_pipe.expect_n_indents(if_keyword_indent_level);
        auto alternative = parse_branching(
            TokenType::ELIF,
            if_keyword_indent_level
        );
        return make_statement(IfBranch(condition, body, alternative));
    }

    if (token_pipe
            .peek_type_at_indent(if_keyword_indent_level, TokenType::ELSE))
    {
        // Consumes the indents
        token_pipe.expect_n_indents(if_keyword_indent_level);
        // Consumes 'else'
        token_pipe.advance_pointer();
        auto alternative = parse_else_block(if_keyword_indent_level);
        return make_statement(IfBranch(condition, body, alternative));
    }

    return make_statement(IfBranch(condition, body));
}

Expression_ptr Parser::parse_ternary_condition(
    TokenType token_type,
    Expression_ptr prev_condition
)
{
    Expression_ptr then_arm = parse_expression();

    if (token_pipe.consume_optional_in_line(TokenType::ELIF))
    {
        auto elif_condition = parse_expression();
        token_pipe.require_in_line(TokenType::THEN);

        auto elif_arm = parse_ternary_condition(
            TokenType::ELIF,
            elif_condition
        );
        return make_expression(
            IfTernaryBranch(prev_condition, then_arm, elif_arm)
        );
    }

    token_pipe.require_in_line(TokenType::ELSE);

    auto expression = parse_expression();
    auto else_arm = make_expression(ElseTernaryBranch(expression));
    return make_expression(IfTernaryBranch(prev_condition, then_arm, else_arm));
}

Statement_ptr Parser::parse_else_block(int if_keyword_indent_level)
{
    token_pipe.require_in_line(TokenType::EOL);

    StatementVector else_block = parse_statements_block(
        if_keyword_indent_level + 1
    );

    return make_statement(ElseBranch{std::move(else_block)});
}

} // namespace Wasp
