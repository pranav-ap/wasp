#include "TokenPipe.h"
#include <optional>
#include <iostream>


using std::vector;
using std::cout;
using std::endl;
using std::nullopt;
using std::make_optional;
using std::optional;

namespace Wasp {
    optional<Token> TokenPipe::current() const {
        if (index >= tokens.size()) return nullopt;
        return tokens[index];
    }

    optional<Token> TokenPipe::lookahead() const {
        if (index + 1 >= tokens.size()) return nullopt;
        return tokens[index + 1];
    }

    Token TokenPipe::require(TokenType token_type) {
        if (const auto token = current()) {
            if (token->type == token_type) {
                advance_pointer();
                return *token;
            }

            cout << "Error: Expected token of type " << to_string(token_type) << " but got " << to_string(
                token->type) << " at line " << token->line << endl;
            exit(1);
        }

        cout << "Error: Expected token of type " << to_string(token_type) << " but got end of file" << endl;
        exit(1);
    }

    bool TokenPipe::is_empty_line() {
        const int start_index = index;

        while (const auto token = current()) {
            if (token->type == TokenType::EOL) {
                advance_pointer();
                return true;
            }

            if (token->type == TokenType::SPACE || token->type == TokenType::TAB) {
                advance_pointer();
                continue;
            }

            break;
        }

        // Reset index after checking for empty line
        index = start_index;
        return false;
    }

    void TokenPipe::ignore_empty_lines() {
        while (is_empty_line()) {
        }
    }

    int TokenPipe::consume_indents() {
        int indent_level = 0;

        while (const auto token = current()) {
            if (token->type == TokenType::TAB) {
                indent_level++;
                advance_pointer();
            } else if (token->type == TokenType::SPACE) {
                cout << "Error: Space detected at line " << token->line << ". Only use tabs for indentation." << endl;
                exit(1);
            } else {
                break;
            }
        }

        return indent_level;
    }

    void TokenPipe::ignore_spaces() {
        while (const auto token = current()) {
            if (token->type == TokenType::SPACE) {
                advance_pointer();
            } else {
                break;
            }
        }
    }

    void TokenPipe::expect_no_indents_or_spaces() const {
        if (const auto token = current(); token->type == TokenType::TAB || token->type == TokenType::SPACE) {
            cout << "Error: Unexpected indent or space at line " << token->line << endl;
            exit(1);
        }
    }

    void TokenPipe::expect_n_indents(const int n) const {
        for (int i = 0; i < n; i++) {
            if (const auto token = current(); token->type != TokenType::TAB) {
                cout << "Error: Expected " << n << " indents but got " << i << " at line " << token->line << endl;
                exit(1);
            }
        }
    }

    // UTILS

    int TokenPipe::get_current_index() const {
        return index;
    }

    void TokenPipe::advance_pointer() {
        index++;
    }

    void TokenPipe::retreat_pointer() {
        index--;
    }

    size_t TokenPipe::get_size() const {
        return tokens.size();
    }
}