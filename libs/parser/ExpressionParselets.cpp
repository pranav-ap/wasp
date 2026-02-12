#include "ExpressionParselets.h"
#include "Parser.h"
#include <cmath>
#include <iostream>

#define MAKE_EXPRESSION(x) std::make_shared<Expression>(Expression{x})

namespace Wasp {
    Expression_ptr IdentifierParselet::parse(Parser &parser, const Token &token) {
        parser.token_pipe.advance_pointer();
        return MAKE_EXPRESSION(Identifier{ token.value });
    }

    Expression_ptr LiteralParselet::parse(Parser &parser, const Token &token) {
        parser.token_pipe.advance_pointer();

        switch (token.type) {
            case TokenType::TRUE_KEYWORD: return MAKE_EXPRESSION(true);
            case TokenType::FALSE_KEYWORD: return MAKE_EXPRESSION(false);
            case TokenType::STRING_LITERAL: return MAKE_EXPRESSION(token.value);
            case TokenType::NUMBER_LITERAL: {
                double value = std::stod(token.value);
                // Check if it's an integer
                if (std::fmod(value, 1.0) == 0.0) {
                    return MAKE_EXPRESSION(static_cast<int>(value));
                }
                return MAKE_EXPRESSION(value);
            }
            default: {
                std::cerr << "Error: Expected a literal value" << std::endl;
                exit(1);
            }
        }
    }

    Expression_ptr PrefixOperatorParselet::parse(Parser &parser, const Token &token) {
        parser.token_pipe.advance_pointer();
        const auto right = parser.parse_expression();

        return MAKE_EXPRESSION((
            Prefix {token, right}
        ));
    }

    int PrefixOperatorParselet::get_precedence() const {
        return precedence;
    }

    Expression_ptr InfixOperatorParselet::parse(Parser &parser, const Expression_ptr left, const Token &token) {
        const auto right = parser.parse_expression(precedence - (is_right_associative ? 1 : 0));

        return MAKE_EXPRESSION((
            Infix {left, token, right}
        ));
    }

    int InfixOperatorParselet::get_precedence() const {
        return precedence;
    }
}
