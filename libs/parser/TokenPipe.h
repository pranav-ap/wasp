#pragma once

#include "Token.h"
#include <vector>
#include <optional>

namespace Wasp {
    class TokenPipe {
        std::vector<Token> tokens;
        int index;

    public:
        explicit TokenPipe() : tokens({}), index(0) {
        };

        explicit TokenPipe(const std::vector<Token> &tokens) : tokens(tokens), index(0) {
        };

        [[nodiscard]] std::optional<Token> current() const;
        [[nodiscard]] std::optional<Token> current_in_line();
        [[nodiscard]] std::optional<Token> later();
        [[nodiscard]] std::optional<Token> lookahead() const;

        // space & indent sensitive 
        Token require(TokenType token_type);
        Token require(const std::vector<TokenType>& token_types);
        
        // ignores spaces and indents 
        // but requires the token to be present later in the line
        Token require_in_line(TokenType token_type);
        Token require_in_line(const std::vector<TokenType>& token_types);

        // ignores spaces, indents and EOL 
        // but requires the token to be present later
        Token require_later(TokenType token_type);
        Token require_later(const std::vector<TokenType>& token_types);

        std::optional<Token> consume_optional(TokenType token_type);
        std::optional<Token> consume_optional_in_line(TokenType token_type);
        std::optional<Token> consume_optional_later(TokenType token_type);

        int consume_indents();

        void ignore_spaces();
        void ignore_spaces_tabs();
        void ignore_spaces_tabs_eols();

        bool is_empty_line();
        void ignore_empty_lines();

        void expect_no_indents_or_spaces() const;
        void expect_n_indents(int n);
        
        int lookahead_indents() const;
        [[nodiscard]] bool peek_type_at_indent(int n, TokenType type) const;
        // Utils

        [[nodiscard]] int get_current_index() const;

        void advance_pointer();
        void retreat_pointer();

        [[nodiscard]] size_t get_size() const;
    };
}