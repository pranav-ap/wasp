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
    token_pipe.ignore_empty_lines();
    token_pipe.expect_n_indents(expected_indent_level);

    const auto token = token_pipe.current();
    if (!token)
        return nullptr;

    if (token->type == TokenType::END_OF_FILE)
    {
        token_pipe.advance_pointer();
        return nullptr;
    }

    if (token->type == TokenType::COMMENT)
    {
        token_pipe.advance_pointer();
        return parse_statement(expected_indent_level);
    }

    switch (token->type)
    {
    case TokenType::TYPE:
        return parse_alias_definition();
    case TokenType::ENUM:
        return parse_enum_definition();

    case TokenType::IF:
        return parse_branching(token->type, expected_indent_level);

    case TokenType::WHILE:
    case TokenType::UNLESS:
    case TokenType::UNTIL:
        return parse_simple_loop(token->type, expected_indent_level);
    case TokenType::FOR:
        return parse_for_in_loop(expected_indent_level);

    case TokenType::PASS:
        return parse_pass_statement();
    case TokenType::NATIVE:
        return parse_native_statement();

    case TokenType::BREAK:
    case TokenType::CONTINUE:
    case TokenType::REDO:
        return parse_loop_control_statement(token->type);

    case TokenType::FUN: {
        token_pipe.advance_pointer();
        return parse_function_definition(expected_indent_level, false, false, false);
    }
    case TokenType::PURE: {
        token_pipe.advance_pointer();
        token_pipe.require_in_line(TokenType::FUN);
        return parse_function_definition(expected_indent_level, false, false, true);
    }
    case TokenType::OPERATOR: {
        token_pipe.advance_pointer();
        return parse_operator_definition(expected_indent_level);
    }

    case TokenType::RETURN_KEYWORD:
        return parse_return_statement();

    case TokenType::CLASS:
        return parse_class_definition(expected_indent_level);

    case TokenType::TRAIT:
        return parse_trait_definition(expected_indent_level);

    case TokenType::TEMPLATE:
        return parse_template_definition(expected_indent_level);

    case TokenType::IMPORT:
        return parse_import();

    default:
        return parse_expression_statement();
    }
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
            int line = current_token ? current_token->line : 0;
            int col = current_token ? current_token->column : 0;

            Doctor::get().fatal(
                WaspStage::Parser,
                "Unexpected indent level. Expected " + std::to_string(expected_indent_level) +
                    " but got " + std::to_string(actual_indent_level),
                line,
                col
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

Statement_ptr Parser::parse_expression_statement() {
    const auto expression = parse_expression();
    token_pipe.require_in_line(TokenType::EOL);

    return make_statement(ExpressionStatement{expression});
}

Statement_ptr Parser::parse_pass_statement() {
    token_pipe.advance_pointer();
    token_pipe.require_in_line(TokenType::EOL);

    return make_statement(Pass{});
}

Statement_ptr Parser::parse_native_statement()
{
    token_pipe.advance_pointer();
    token_pipe.require_in_line(TokenType::EOL);

    return make_statement(Native{});
}

Statement_ptr Parser::parse_return_statement() {
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
    auto sym_token = token_pipe.require_in_line(TokenType::IDENTIFIER);
    std::optional<std::string> alias = std::nullopt;

    if (token_pipe.consume_optional_in_line(TokenType::AS))
    {
        alias = token_pipe.require_in_line(TokenType::IDENTIFIER).value;
    }

    return {sym_token.value, std::move(alias)};
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
            access_argument = std::stoi(num_token.value);
            token_pipe.require_in_line(TokenType::CLOSE_PARENTHESIS);
        }

        token_pipe.require_in_line(TokenType::DOT);
    }

    do
    {
        path.push_back(token_pipe.require_in_line(TokenType::IDENTIFIER).value);
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
        module_alias = token_pipe.require_in_line(TokenType::IDENTIFIER).value;
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
                        token_pipe.require_in_line(TokenType::IDENTIFIER).value
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

} // namespace Wasp
