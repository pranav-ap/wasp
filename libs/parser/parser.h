#pragma once

#include "Expression.h"
#include "Statement.h"
#include "Precedence.h"
#include "ExpressionParselets.h"
#include "Token.h"
#include "TokenPipe.h"

#include <vector>
#include <memory>
#include <map>
#include <optional>


namespace Wasp {
    struct Module {
        std::vector<Statement_ptr> statements;
    };

    class Parser {
        int indent_level = 0;

        // Statement Parser

        Statement_ptr parse_statement();

        Statement_ptr parse_expression_statement();

        // Pratt Parser

        std::map<TokenType, IPrefixParselet_ptr> prefix_parselets;
        std::map<TokenType, IInfixParselet_ptr> infix_parselets;

        void register_parselet(TokenType token_type, IPrefixParselet_ptr parselet);

        void register_parselet(TokenType token_type, IInfixParselet_ptr parselet);

        void register_prefix(TokenType token_type, Precedence precedence);

        void register_infix_left(TokenType token_type, Precedence precedence);

        void register_infix_right(TokenType token_type, Precedence precedence);

        [[nodiscard]] int get_next_operator_precedence();

    public:
        TokenPipe token_pipe;

        Parser();

        // Expression Parser

        Expression_ptr parse_expression();

        Expression_ptr parse_expression(int precedence);

        ExpressionVector parse_expressions();

        Module run(const std::vector<Token> &tokens);
    };

    using ParserPtr = std::shared_ptr<Parser>;
}