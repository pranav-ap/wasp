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
        [[nodiscard]] std::optional<Token> lookahead() const;

        Token require(TokenType token_type);
        Token require(const std::vector<TokenType>& token_types);
        std::optional<Token> consume_optional(TokenType token_type);

        bool is_empty_line();

        void ignore_empty_lines();

        int consume_indents();

        void ignore_spaces();

        void expect_no_indents_or_spaces() const;

        void expect_n_indents(int n);

        // Utils

        [[nodiscard]] int get_current_index() const;

        void advance_pointer();

        void retreat_pointer();

        [[nodiscard]] size_t get_size() const;
    };
}