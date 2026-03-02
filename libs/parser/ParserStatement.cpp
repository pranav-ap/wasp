#include "parser.h"

#include <vector>
#include <stack>
#include <memory>
#include <utility>
#include <iostream>
#include <map>

#define RETURN_IF_NULLOPT(token) \
    if (!token.has_value())      \
        return nullptr;
#define EXIT_IF_NULLOPT(token) \
    if (!token.has_value())    \
        exit(1);
#define RETURN_IF_NULLPTR(token) \
    if (!token)                  \
        return nullptr;
#define EXIT_IF_NULLPTR(token) \
    if (!token)                \
        exit(1);
#define CASE(token_type, call) \
    case token_type:           \
    {                          \
        return call;           \
    }
#define MAKE_STATEMENT(x) std::make_shared<Statement>(Statement(x))
#define MAKE_EXPRESSION(x) std::make_shared<Expression>(Expression(x))

using std::cout;
using std::endl;
using std::make_pair;
using std::make_shared;
using std::move;
using std::optional;

namespace Wasp
{
    Statement_ptr Parser::parse_statement(int expected_indent_level)
    {
        token_pipe.ignore_empty_lines();
        token_pipe.expect_n_indents(expected_indent_level);

        const auto token = token_pipe.current();
        RETURN_IF_NULLOPT(token);

        if (token.value().type == TokenType::END_OF_FILE)
        {
            token_pipe.advance_pointer();
            return nullptr;
        }

        switch (token->type)
        {
            CASE(TokenType::LET, parse_variable_definition(true));
            CASE(TokenType::CONST_KEYWORD, parse_variable_definition(false));
            CASE(TokenType::TYPE, parse_alias_definition());
            CASE(TokenType::ENUM, parse_enum_definition());

            CASE(TokenType::IF, parse_branching(token.value().type, expected_indent_level));

            CASE(TokenType::WHILE, parse_simple_loop(TokenType::WHILE, expected_indent_level));
            CASE(TokenType::UNLESS, parse_simple_loop(TokenType::UNLESS, expected_indent_level));
            CASE(TokenType::UNTIL, parse_simple_loop(TokenType::UNTIL, expected_indent_level));
            CASE(TokenType::FOR, parse_for_in_loop(expected_indent_level));

            CASE(TokenType::PASS, parse_pass_statement());

            CASE(TokenType::BREAK, parse_loop_control_statement(TokenType::BREAK));
            CASE(TokenType::CONTINUE, parse_loop_control_statement(TokenType::CONTINUE));
            CASE(TokenType::REDO, parse_loop_control_statement(TokenType::REDO));

            CASE(TokenType::FUN, parse_function_definition(expected_indent_level));
            CASE(TokenType::RETURN_KEYWORD, parse_return_statement());

            CASE(TokenType::AT_SIGN, parse_annotation_definition());

            CASE(TokenType::CLASS, parse_class_definition(expected_indent_level));
            CASE(TokenType::TRAIT, parse_trait_definition(expected_indent_level));
            CASE(TokenType::IMPL, parse_impl_definition(expected_indent_level));

        default:
            return parse_expression_statement();
        }
    }

    Block Parser::parse_statements_block(int expected_indent_level)
    {
        token_pipe.ignore_empty_lines();

        auto s = parse_statement(expected_indent_level);
        EXIT_IF_NULLPTR(s);

        Block statements{move(s)};

        while (true)
        {
            token_pipe.ignore_empty_lines();
            int actual_indent_level = token_pipe.lookahead_indents();

            if (actual_indent_level > expected_indent_level)
            {
                cout << "Unexpected indent level. Expected " << expected_indent_level << " but got " << actual_indent_level << endl;
                exit(1);
            }

            if (actual_indent_level == expected_indent_level)
            {
                auto s = parse_statement(expected_indent_level);
                if (!s)
                    break;

                statements.push_back(move(s));
                continue;
            }

            break;
        }

        return statements;
    }

    Statement_ptr Parser::parse_expression_statement()
    {
        const auto expression = parse_expression();
        token_pipe.require_in_line(TokenType::EOL);

        return MAKE_STATEMENT((ExpressionStatement{
            expression}));
    }

    Statement_ptr Parser::parse_pass_statement()
    {
        token_pipe.advance_pointer();
        token_pipe.require_in_line(TokenType::EOL);

        return MAKE_STATEMENT((Pass{}));
    }

    Statement_ptr Parser::parse_return_statement()
    {
        token_pipe.advance_pointer();

        if (token_pipe.consume_optional_in_line(TokenType::EOL))
        {
            return MAKE_STATEMENT((Return{}));
        }

        token_pipe.ignore_spaces_tabs();

        auto expression = parse_expression();
        token_pipe.require_in_line(TokenType::EOL);
        return MAKE_STATEMENT((Return{expression}));
    }
}
