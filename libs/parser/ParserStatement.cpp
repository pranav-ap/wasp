#include "Doctor.h"
#include "Parser.h"
#include "Statement.h"
#include "Token.h"

#include <cstdlib>
#include <memory>
#include <optional>
#include <string>
#include <utility>

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
        return parse_function_definition(expected_indent_level);
    case TokenType::RETURN_KEYWORD:
        return parse_return_statement();

    case TokenType::AT_SIGN:
        return parse_annotation_definition();

    case TokenType::CLASS:
        return parse_class_definition(expected_indent_level);
    case TokenType::TRAIT:
        return parse_trait_definition(expected_indent_level);
    case TokenType::IMPL:
        return parse_impl_definition(expected_indent_level);

    default:
        return parse_expression_statement();
    }
}

Block Parser::parse_statements_block(int expected_indent_level) {
    token_pipe.ignore_empty_lines();

    auto s = parse_statement(expected_indent_level);
    Doctor::get().fatal_if_nullptr(s, WaspStage::Parser);

    Block statements{std::move(s)};

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

    return std::make_shared<Statement>(ExpressionStatement{expression});
}

Statement_ptr Parser::parse_pass_statement() {
    token_pipe.advance_pointer();
    token_pipe.require_in_line(TokenType::EOL);

    return std::make_shared<Statement>(Pass{});
}

Statement_ptr Parser::parse_return_statement() {
    token_pipe.advance_pointer();

    if (token_pipe.consume_optional_in_line(TokenType::EOL)) {
        return std::make_shared<Statement>(Return{});
    }

    token_pipe.ignore_spaces_tabs();

    auto expression = parse_expression();
    token_pipe.require_in_line(TokenType::EOL);
    return std::make_shared<Statement>(Return{expression});
}
} // namespace Wasp