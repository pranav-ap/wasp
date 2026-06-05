#include "AST.h"
#include "Doctor.h"
#include "Parser.h"
#include "Statement.h"
#include "Token.h"

#include <cstdlib>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace Wasp {

Statement_ptr Parser::parse_statement(int expected_indent_level)
{
    if (!skip_to_statement(expected_indent_level))
    {
        return nullptr;
    }

    consume_indents(expected_indent_level);

    const auto token = token_pipe.current();
    if (!token || token->type == TokenType::END_OF_FILE)
    {
        token_pipe.advance_pointer();
        return nullptr;
    }

    Statement_ptr result = nullptr;

    switch (token->type)
    {
    case TokenType::TYPE:
        result = parse_alias_definition();
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

    case TokenType::NATIVE:
    case TokenType::REQUIRED:
        result = parse_placeholder(token->type);
        break;

    case TokenType::BREAK:
    case TokenType::CONTINUE:
    case TokenType::REDO:
        result = parse_loop_control_statement(token->type);
        break;

    case TokenType::FUN: {
        token_pipe.advance_pointer();
        result = parse_function_definition(
            expected_indent_level,
            false,
            false,
            false
        );
        break;
    }
    case TokenType::PURE: {
        token_pipe.advance_pointer();
        token_pipe.require_in_line(TokenType::FUN);
        result = parse_function_definition(
            expected_indent_level,
            false,
            false,
            true
        );
        break;
    }
    case TokenType::INFIX:
    case TokenType::PREFIX:
    case TokenType::POSTFIX: {
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

    skip_trailing_comment();

    return result;
}

StatementVector Parser::parse_statements_block(int expected_indent_level) {
    token_pipe.ignore_empty_lines();

    auto s = parse_statement(expected_indent_level);
    Doctor::get().fatal_if_nullptr(s, WaspStage::Parser);

    StatementVector statements{std::move(s)};

    while (true) {
        token_pipe.ignore_empty_lines();
        int actual_indent_level = token_pipe.lookahead_indents();

        if (actual_indent_level > expected_indent_level) {
            auto current_token = token_pipe.current();

            Doctor::get().fatal(
                WaspStage::Parser,
                "Unexpected indent level. Expected " +
                    std::to_string(expected_indent_level) + " but got " +
                    std::to_string(actual_indent_level)
            );
        }

        if (actual_indent_level == expected_indent_level) {
            auto parsed_stmt = parse_statement(expected_indent_level);
            if (!parsed_stmt)
                break;

            statements.push_back(std::move(parsed_stmt));
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

    return make_statement(ExpressionStatement{expression});
}

Statement_ptr Parser::parse_placeholder(TokenType placeholder_type)
{
    token_pipe.advance_pointer();
    token_pipe.require_in_line(TokenType::EOL);

    return make_statement(Placeholder(placeholder_type));
}

Statement_ptr Parser::parse_return_statement()
{
    token_pipe.advance_pointer();

    if (token_pipe.consume_optional_in_line(TokenType::EOL)) {
        return make_statement(Return{});
    }

    token_pipe.ignore_spaces_tabs();

    auto expression = parse_expression();
    token_pipe.require_in_line(TokenType::EOL);
    return make_statement(Return{expression});
}

// Imports

ImportAsPair Parser::parse_imported_symbol()
{
    auto name = token_pipe.require_in_line(TokenType::IDENTIFIER).lexeme;

    if (token_pipe.consume_optional_in_line(TokenType::AS))
    {
        return ImportAsPair(
            std::move(name),
            token_pipe.require_in_line(TokenType::IDENTIFIER).lexeme
        );
    }

    return ImportAsPair(std::move(name));
}

std::tuple<std::optional<TokenType>, int, StringVector> Parser::
    parse_module_path()
{
    std::optional<TokenType> access_modifier = std::nullopt;
    int access_argument = 1; // Defaults to 1 for standard my/our/pkg/top/up
    StringVector path;

    auto current = token_pipe.current_in_line();

    if (current &&
        (current->type == TokenType::MY || current->type == TokenType::OUR ||
         current->type == TokenType::UP || current->type == TokenType::PKG ||
         current->type == TokenType::TOP))
    {
        access_modifier = current->type;
        token_pipe.advance_pointer();

        // Check for the integer argument, e.g., up(2)
        if (token_pipe.consume_optional_in_line(TokenType::OPEN_PARENTHESIS))
        {
            auto num_token = token_pipe.require_in_line(
                TokenType::NUMBER_LITERAL
            );
            access_argument = std::stoi(num_token.lexeme);
            token_pipe.require_in_line(TokenType::CLOSE_PARENTHESIS);
        }

        token_pipe.require_in_line(TokenType::DOT);
    }

    do
    {
        path.push_back(
            token_pipe.require_in_line(TokenType::IDENTIFIER).lexeme
        );
    }
    while (token_pipe.consume_optional_in_line(TokenType::DOT));

    return {access_modifier, access_argument, path};
}

Statement_ptr Parser::parse_import()
{
    // Consume 'import'
    token_pipe.advance_pointer();

    auto [access_modifier, access_arg, path] = parse_module_path();

    std::optional<std::string> module_alias = std::nullopt;
    bool expose_all = false;
    std::vector<ImportAsPair> exposed_symbols;
    StringVector excluded_symbols;

    // Check for module alias (import X as x)
    if (token_pipe.consume_optional_in_line(TokenType::AS))
    {
        module_alias = token_pipe.require_in_line(TokenType::IDENTIFIER).lexeme;
    }

    // Check for exposed symbols (expose a, b, c)
    if (token_pipe.consume_optional_in_line(TokenType::EXPOSE))
    {
        // Expose everything (*)
        if (token_pipe.consume_optional_in_line(TokenType::STAR))
        {
            expose_all = true;

            // Expose everything EXCEPT specific symbols
            if (token_pipe.consume_optional_in_line(TokenType::EXCEPT))
            {
                do
                {
                    excluded_symbols.push_back(
                        token_pipe.require_in_line(TokenType::IDENTIFIER).lexeme
                    );
                }
                while (token_pipe.consume_optional_in_line(TokenType::COMMA));
            }
        }
        // Expose specific symbols explicitly
        else
        {
            do
            {
                exposed_symbols.push_back(parse_imported_symbol());
            }
            while (token_pipe.consume_optional_in_line(TokenType::COMMA));
        }
    }

    token_pipe.require_in_line(TokenType::EOL);

    return make_statement(Import(
        access_modifier,
        access_arg,
        std::move(path),
        std::move(module_alias),
        expose_all,
        std::move(exposed_symbols),
        std::move(excluded_symbols)
    ));
}

// Comments

bool Parser::skip_to_statement(int expected_indent_level)
{
    while (true)
    {
        // Skip empty lines
        token_pipe.ignore_empty_lines();

        // Check if we have the expected indentation level without consuming
        if (token_pipe.lookahead_indents() != expected_indent_level)
        {
            return false;
        }

        // Check if we're at a comment (after indentation)
        if (token_pipe.current() &&
            token_pipe.current()->type == TokenType::COMMENT)
        {
            skip_comment_line();
            continue;
        }
        break;
    }
    return true;
}

void Parser::skip_comment_line()
{
    // Skip the comment token
    token_pipe.advance_pointer();

    // Skip the rest of the line after the comment
    while (token_pipe.current() && token_pipe.current()->type != TokenType::EOL)
    {
        token_pipe.advance_pointer();
    }

    // Skip the EOL
    if (token_pipe.current() && token_pipe.current()->type == TokenType::EOL)
    {
        token_pipe.advance_pointer();
    }
}

void Parser::consume_indents(int expected_indent_level)
{
    int consumed = 0;
    int space_buffer = 0;

    while (consumed < expected_indent_level)
    {
        auto token = token_pipe.current();
        if (!token)
        {
            break;
        }

        if (token->type == TokenType::TAB)
        {
            consumed++;
            token_pipe.advance_pointer();
        }
        else if (token->type == TokenType::SPACE)
        {
            space_buffer++;
            token_pipe.advance_pointer();
            if (space_buffer == 4)
            {
                consumed++;
                space_buffer = 0;
            }
        }
        else
        {
            break;
        }
    }

    // Verify we consumed the expected number of indents
    Doctor::get().assert(
        consumed == expected_indent_level,
        WaspStage::Parser,
        "Expected " + std::to_string(expected_indent_level) +
            " indents but consumed " + std::to_string(consumed)
    );
}

void Parser::skip_trailing_comment()
{
    token_pipe.ignore_spaces_tabs();
    if (token_pipe.current() &&
        token_pipe.current()->type == TokenType::COMMENT)
    {
        token_pipe.advance_pointer(); // Skip the comment
        // Consume until EOL
        while (token_pipe.current() &&
               token_pipe.current()->type != TokenType::EOL)
        {
            token_pipe.advance_pointer();
        }
    }
}

} // namespace Wasp
