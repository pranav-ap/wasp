#include "TokenPipe.h"
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

    optional<Token> TokenPipe::current_in_line() {
        ignore_spaces_tabs();
        return current();
    }

    optional<Token> TokenPipe::later() {
        ignore_spaces_tabs_eols();
        return current();
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

    Token TokenPipe::require(const std::vector<TokenType>& token_types) {
        const auto token = current();
        
        if (token) {
            for (const auto& type : token_types) {
                if (token->type == type) {
                    advance_pointer();
                    return *token;
                }
            }

            // Error handling if no match is found
            std::cout << "Error: Expected one of { ";
            for (size_t i = 0; i < token_types.size(); ++i) {
                std::cout << to_string(token_types[i]) << (i < token_types.size() - 1 ? ", " : " ");
            }
            std::cout << "} but got " << to_string(token->type) 
                    << " at line " << token->line << std::endl;
            exit(1);
        }

        std::cout << "Error: Expected one of multiple types but got end of file" << std::endl;
        exit(1);
    }

    Token TokenPipe::require_in_line(TokenType token_type) {
        ignore_spaces_tabs();
        return require(token_type);
    }

    Token TokenPipe::require_in_line(const std::vector<TokenType>& token_types) {
        ignore_spaces_tabs();
        return require(token_types);
    }

    Token TokenPipe::require_later(TokenType token_type) {
        ignore_spaces_tabs_eols();
        return require(token_type);
    }

    Token TokenPipe::require_later(const std::vector<TokenType>& token_types) {
        ignore_spaces_tabs_eols();
        return require(token_types);
    }

    optional<Token> TokenPipe::consume_optional(TokenType token_type) {
	    auto token = current();

    	if (token.has_value() && token.value().type == token_type){
	    	advance_pointer();
		    return token;
	    }

	    return nullopt;
    }

    optional<Token> TokenPipe::consume_optional_in_line(TokenType token_type) {
        ignore_spaces_tabs();
        return consume_optional(token_type);
    }

    optional<Token> TokenPipe::consume_optional_later(TokenType token_type) {
        ignore_spaces_tabs_eols();
        return consume_optional(token_type);
    }

    int TokenPipe::consume_indents() {
        int indent_level = 0;

        while (const auto token = current()) {
            if (token->type == TokenType::TAB) {
                indent_level++;
                advance_pointer();
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

    void TokenPipe::ignore_spaces_tabs() {
        while (const auto token = current()) {
            if (token->type == TokenType::SPACE || token->type == TokenType::TAB) {
                advance_pointer();
            } else {
                break;
            }
        }
    }

    void TokenPipe::ignore_spaces_tabs_eols() {
        while (const auto token = current()) {
            if (token->type == TokenType::SPACE || token->type == TokenType::TAB || token->type == TokenType::EOL) {
                advance_pointer();
            } else {
                break;
            }
        }
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

    void TokenPipe::expect_no_indents_or_spaces() const {
        if (const auto token = current(); token->type == TokenType::TAB || token->type == TokenType::SPACE) {
            cout << "Error: Unexpected indent or space at line " << token->line << endl;
            exit(1);
        }
    }

    void TokenPipe::expect_n_indents(const int n) {
        for (int i = 0; i < n; i++) {
            if (const auto token = current(); token->type != TokenType::TAB) {
                cout << "Error: Expected " << n << " indents but got " << i << " at line " << token->line << endl;
                exit(1);
            }

            advance_pointer();
        }
    }

    int TokenPipe::lookahead_indents() const {
        int indent_count = 0;
        int space_buffer = 0;
        size_t temp_index = index;

        while (temp_index < tokens.size()) {
            const Token& token = tokens[temp_index];

            if (token.type == TokenType::TAB) {
                indent_count++;
                temp_index++;
            } 
            else if (token.type == TokenType::SPACE) {
                space_buffer++;
                temp_index++;
                if (space_buffer == 4) {
                    indent_count++;
                    space_buffer = 0;
                }
            } 
            else {
                // Found a non-indentation token
                break;
            }
        }

        return indent_count;
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