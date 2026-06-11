#include "Statement.h"
#include "AST.h"
#include "Doctor.h"
#include "Parser.h"
#include "Token.h"

#include <cstdlib>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace Wasp
{

Statement_ptr Parser::parse_statement(int expected_indent_level)
{
    token_pipe.ignore_empty_lines();
    token_pipe.expect_n_indents(expected_indent_level);

    const auto token = token_pipe.current();

    if (!token)
    {
        return nullptr;
    }

    if (token->type == TokenType::END_OF_FILE)
    {
        token_pipe.advance_pointer();
        return nullptr;
    }

    Statement_ptr result = nullptr;

    switch (token->type)
    {
    case TokenType::TYPE:
        result = parse_type_alias_definition();
        break;
    case TokenType::ENUM:
        result = parse_enum_definition();
        break;

    case TokenType::IF:
        result = parse_branching(token->type, expected_indent_level);
        break;

    case TokenType::WHILE:
    case TokenType::UNLESS:
    case TokenType::UNTIL:
        result = parse_simple_loop(token->type, expected_indent_level);
        break;
    case TokenType::FOR:
        result = parse_for_in_loop(expected_indent_level);
        break;

    case TokenType::NATIVE: {
        token_pipe.advance_pointer();
        token_pipe.require_in_line(TokenType::EOL);

        return make_statement(Native());
    }
    case TokenType::REQUIRED: {
        token_pipe.advance_pointer();
        token_pipe.require_in_line(TokenType::EOL);

        return make_statement(Required());
    }
    case TokenType::PASS: {
        token_pipe.advance_pointer();
        token_pipe.require_in_line(TokenType::EOL);

        return make_statement(Pass());
    }

    case TokenType::BREAK:
    case TokenType::CONTINUE:
    case TokenType::REDO:
        result = parse_loop_control_statement(token->type);
        break;

    case TokenType::FUN: {
        token_pipe.advance_pointer();
        result = parse_function_definition(expected_indent_level, false, false);
        break;
    }
    case TokenType::PURE: {
        token_pipe.advance_pointer();
        token_pipe.require_in_line(TokenType::FUN);
        result = parse_function_definition(expected_indent_level, false, true);
        break;
    }

    case TokenType::SHARE: {
        token_pipe.advance_pointer();
        bool is_pure = token_pipe.consume_optional_in_line(TokenType::PURE)
                           .has_value();

        token_pipe.require_in_line(TokenType::FUN);
        result = parse_function_definition(
            expected_indent_level,
            true,
            is_pure
        );
        break;
    }

    case TokenType::INFIX:
    case TokenType::PREFIX: {
        TokenType fixity = token_pipe.current()->type;
        token_pipe.advance_pointer();
        result = parse_operator_definition(fixity, expected_indent_level);
        break;
    }

    case TokenType::RETURN_KEYWORD:
        result = parse_return_statement();
        break;

    case TokenType::CLASS:
        result = parse_class_definition(expected_indent_level);
        break;

    case TokenType::TRAIT:
        result = parse_trait_definition(expected_indent_level);
        break;

    case TokenType::TEMPLATE:
        result = parse_template_definition(expected_indent_level);
        break;

    case TokenType::IMPORT:
        result = parse_import();
        break;

    default:
        result = parse_expression_statement();
        break;
    }

    return result;
}

Block Parser::parse_block(int expected_indent_level)
{
    token_pipe.ignore_empty_lines();

    auto s = parse_statement(expected_indent_level);
    Doctor::get().fatal_if_nullptr(s, WaspStage::Parser);

    StatementVector statements{std::move(s)};

    while (true)
    {
        token_pipe.ignore_empty_lines();
        int actual_indent_level = token_pipe.lookahead_indents();

        if (actual_indent_level > expected_indent_level)
        {
            auto current_token = token_pipe.current();

            Doctor::get().fatal(
                WaspStage::Parser,
                "Unexpected indent level. Expected " +
                    std::to_string(expected_indent_level) + " but got " +
                    std::to_string(actual_indent_level)
            );
        }

        if (actual_indent_level == expected_indent_level)
        {
            auto parsed_stmt = parse_statement(expected_indent_level);
            if (!parsed_stmt)
            {
                break;
            }

            statements.push_back(std::move(parsed_stmt));
            continue;
        }

        break;
    }

    return Block{std::move(statements)};
}

Statement_ptr Parser::parse_expression_statement()
{
    const auto expression = parse_expression();
    token_pipe.require_in_line(TokenType::EOL);

    return make_statement(ExpressionStatement{expression});
}

} // namespace Wasp
