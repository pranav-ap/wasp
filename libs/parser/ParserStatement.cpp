#include "AST.h"
#include "Doctor.h"
#include "Parser.h"
#include "Statement.h"
#include "Token.h"

#include <cstdlib>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace Wasp {
Statement_ptr Parser::parse_statement(int expected_indent_level) {
    token_pipe.ignore_empty_lines();
    token_pipe.expect_n_indents(expected_indent_level);

    const auto token = token_pipe.current();
    if (!token)
        return nullptr;

    if (token->type == TokenType::END_OF_FILE) {
        token_pipe.advance_pointer();
        return nullptr;
    }

    switch (token->type) {
    case TokenType::LET:
        return parse_variable_definition(true);
    case TokenType::CONST_KEYWORD:
        return parse_variable_definition(false);
    case TokenType::TYPE:
        return parse_alias_definition();
    case TokenType::ENUM:
        return parse_enum_definition();

    case TokenType::IF:
        return parse_branching(token->type, expected_indent_level);

    case TokenType::WHILE:
        return parse_simple_loop(TokenType::WHILE, expected_indent_level);
    case TokenType::UNLESS:
        return parse_simple_loop(TokenType::UNLESS, expected_indent_level);
    case TokenType::UNTIL:
        return parse_simple_loop(TokenType::UNTIL, expected_indent_level);
    case TokenType::FOR:
        return parse_for_in_loop(expected_indent_level);

    case TokenType::PASS:
        return parse_pass_statement();

    case TokenType::BREAK:
        return parse_loop_control_statement(TokenType::BREAK);
    case TokenType::CONTINUE:
        return parse_loop_control_statement(TokenType::CONTINUE);
    case TokenType::REDO:
        return parse_loop_control_statement(TokenType::REDO);

    case TokenType::FUN:
        return parse_function_definition(expected_indent_level, false);
    case TokenType::PURE:
        return parse_function_definition(expected_indent_level, false, true);
    case TokenType::RETURN_KEYWORD:
        return parse_return_statement();

    case TokenType::AT_SIGN:
        return parse_annotation_definition();

    case TokenType::CLASS:
        return parse_class_definition(expected_indent_level);

    case TokenType::IMPORT:
        return parse_import();
    case TokenType::FROM:
        return parse_from_import();

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

std::pair<std::optional<TokenType>, std::vector<std::string>> Parser::parse_module_path() {
    std::optional<TokenType> access_token = std::nullopt;
    std::vector<std::string> path;

    auto token = token_pipe.current_in_line();
    Doctor::get().fatal_if_nullopt(token, WaspStage::Parser);

    bool is_access_modifier = false;

    switch (token->type) {
    case TokenType::TOP:
    case TokenType::PKG:
    case TokenType::MY:
    case TokenType::OUR:
    case TokenType::UP:
        access_token = token->type;
        is_access_modifier = true;
        break;
    default:
        is_access_modifier = false;
        break;
    }

    if (is_access_modifier) {
        token_pipe.advance_pointer();

        // Handle parameterized jumps like up(2) or pkg(3)
        if (access_token == TokenType::UP || access_token == TokenType::PKG) {
            if (token_pipe.consume_optional_in_line(TokenType::OPEN_PARENTHESIS)) {
                auto num = token_pipe.require_in_line(TokenType::NUMBER_LITERAL);
                // Store depth as the first path element
                path.push_back(num.value);
                token_pipe.require_in_line(TokenType::CLOSE_PARENTHESIS);
            } else {
                // Default depth
                path.push_back("0");
            }
        }

        // If a dot follows, consume it and continue.
        if (!token_pipe.consume_optional_in_line(TokenType::DOT)) {
            return {access_token, path};
        }
    }

    // Parse the rest of the dot-separated path
    while (true) {
        auto part = token_pipe.require_in_line(TokenType::IDENTIFIER);
        path.push_back(part.value);

        if (!token_pipe.consume_optional_in_line(TokenType::DOT)) {
            break;
        }
    }

    return {access_token, path};
}

Statement_ptr Parser::parse_import() {
    token_pipe.advance_pointer();

    auto [access_token, path] = parse_module_path();
    std::optional<std::string> alias = std::nullopt;

    if (token_pipe.consume_optional_in_line(TokenType::AS)) {
        alias = token_pipe.require_in_line(TokenType::IDENTIFIER).value;
    }

    token_pipe.require_in_line(TokenType::EOL);

    return make_statement(SimpleImport(access_token, std::move(path), std::move(alias)));
}

ImportedSymbol Parser::parse_imported_symbol() {
    auto sym_token = token_pipe.require_in_line(TokenType::IDENTIFIER);
    std::optional<std::string> alias = std::nullopt;

    if (token_pipe.consume_optional_in_line(TokenType::AS)) {
        alias = token_pipe.require_in_line(TokenType::IDENTIFIER).value;
    }

    return {sym_token.value, std::move(alias)};
}

Statement_ptr Parser::parse_from_import() {
    token_pipe.advance_pointer();

    auto [access_token, path] = parse_module_path();

    token_pipe.require_in_line(TokenType::IMPORT);

    std::vector<ImportedSymbol> symbols;

    //  from top.engine import (Tank, Pump as FuelPump)
    if (token_pipe.consume_optional_in_line(TokenType::OPEN_PARENTHESIS)) {
        do {
            token_pipe.ignore_spaces_tabs_eols();
            symbols.push_back(parse_imported_symbol());

        } while (token_pipe.consume_optional_in_line(TokenType::COMMA));

        token_pipe.ignore_empty_lines();
        token_pipe.require(TokenType::CLOSE_PARENTHESIS);
    }
    // from my.fuel import Tank as T
    else {
        symbols.push_back(parse_imported_symbol());
    }

    token_pipe.require_in_line(TokenType::EOL);

    return make_statement(FromImport(access_token, std::move(path), std::move(symbols)));
}
} // namespace Wasp
