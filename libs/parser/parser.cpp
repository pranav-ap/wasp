#include "Parser.h"

#include <vector>
#include <stack>
#include <memory>
#include <utility>
#include <iostream>
#include <map>

#define RETURN_IF_NULLOPT(token) if (!token.has_value()) return nullptr;
#define EXIT_IF_NULLOPT(token) if (!token.has_value()) exit(1);
#define RETURN_IF_NULLPTR(token) if (!token) return nullptr;
#define EXIT_IF_NULLPTR(token) if (!token) exit(1);
#define CASE(token_type, call) case token_type: { return call; }
#define MAKE_STATEMENT(x) std::make_shared<Statement>(Statement(x))
#define MAKE_EXPRESSION(x) std::make_shared<Expression>(Expression(x))

template<class... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};

template<class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

using std::move;
using std::cout;
using std::optional;
using std::endl;
using std::make_pair;

namespace Wasp {
    Parser::Parser() {
        register_parselet(TokenType::IDENTIFIER, std::make_shared<IdentifierParselet>());
        register_parselet(TokenType::STRING_LITERAL, std::make_shared<LiteralParselet>());
        register_parselet(TokenType::NUMBER_LITERAL, std::make_shared<LiteralParselet>());
        register_parselet(TokenType::TRUE_KEYWORD, std::make_shared<LiteralParselet>());
        register_parselet(TokenType::FALSE_KEYWORD, std::make_shared<LiteralParselet>());
        register_parselet(TokenType::NONE, std::make_shared<LiteralParselet>());

        register_prefix(TokenType::PLUS, Precedence::PREFIX);
        register_prefix(TokenType::MINUS, Precedence::PREFIX);
        register_prefix(TokenType::NOT, Precedence::PREFIX);
        register_prefix(TokenType::DOT_DOT_DOT, Precedence::PREFIX);

        register_infix_left(TokenType::PLUS, Precedence::TERM);
        register_infix_left(TokenType::MINUS, Precedence::TERM);
        register_infix_left(TokenType::STAR, Precedence::PRODUCT);
        register_infix_left(TokenType::DIVISION, Precedence::PRODUCT);
        register_infix_left(TokenType::REMINDER, Precedence::PRODUCT);
        register_infix_left(TokenType::EQUAL_EQUAL, Precedence::EQUALITY);
        register_infix_left(TokenType::BANG_EQUAL, Precedence::EQUALITY);
        register_infix_left(TokenType::LESSER_THAN, Precedence::COMPARISON);
        register_infix_left(TokenType::LESSER_THAN_EQUAL, Precedence::COMPARISON);
        register_infix_left(TokenType::GREATER_THAN, Precedence::COMPARISON);
        register_infix_left(TokenType::GREATER_THAN_EQUAL, Precedence::COMPARISON);
        register_infix_left(TokenType::IN_KEYWORD, Precedence::COMPARISON);
        register_infix_left(TokenType::IS, Precedence::COMPARISON);
        register_infix_left(TokenType::AND, Precedence::AND);
        register_infix_left(TokenType::OR, Precedence::OR);

        register_infix_right(TokenType::POWER, Precedence::EXPONENT);
        register_infix_right(TokenType::EQUAL, Precedence::ASSIGNMENT);
    }

    Statement_ptr Parser::parse_statement() {
        token_pipe.ignore_empty_lines();
        token_pipe.expect_no_indents_or_spaces();

        const auto token = token_pipe.current();
        RETURN_IF_NULLOPT(token);

        if (token.value().type == TokenType::END_OF_FILE) {
            token_pipe.advance_pointer();
            return nullptr;
        }

        switch (token->type) {
            default:
                return parse_expression_statement();
        }
    }

    Statement_ptr Parser::parse_expression_statement() {
        const auto expression = parse_expression();
        token_pipe.require(TokenType::EOL);

        return MAKE_STATEMENT((ExpressionStatement {
            expression
        }));
    }

    Expression_ptr Parser::parse_expression() {
        return parse_expression(0);
    }

    Expression_ptr Parser::parse_expression(const int precedence) {
        token_pipe.ignore_spaces();

        auto token = token_pipe.current();
        RETURN_IF_NULLOPT(token);

        if (token.value().type == TokenType::CLOSE_PARENTHESIS) return nullptr;

        const IPrefixParselet_ptr prefix_parselet = prefix_parselets.at(token.value().type);
        EXIT_IF_NULLPTR(prefix_parselet);
        Expression_ptr left = prefix_parselet->parse(*this, token.value());
        EXIT_IF_NULLPTR(left);

        token_pipe.ignore_spaces();

        while (precedence < get_next_operator_precedence()) {
            token = token_pipe.current();
            EXIT_IF_NULLOPT(token);

            token_pipe.advance_pointer();

            const IInfixParselet_ptr infix_parselet = infix_parselets.at(token.value().type);
            left = infix_parselet->parse(*this, left, token.value());
        }

        return left;
    }

    ExpressionVector Parser::parse_expressions() {
        ExpressionVector elements;

        while (auto element = parse_expression())
        {
            elements.push_back(move(element));

            token_pipe.ignore_spaces();

            if (const auto token = token_pipe.current(); token.has_value() && token->type == TokenType::COMMA) {
                token_pipe.advance_pointer();
            }

            break;
        }

        return elements;
    }

    int Parser::get_next_operator_precedence() {
        if (const auto token = token_pipe.current();
            token.has_value() && infix_parselets.contains(token.value().type)) {
            const IInfixParselet_ptr infix_parselet = infix_parselets.at(token.value().type);
            return infix_parselet->get_precedence();
        }

        return 0;
    }

    Module Parser::run(const std::vector<Token> &tokens) {
        token_pipe = TokenPipe(tokens);
        Module mod;

        const auto tokens_count = token_pipe.get_size();
        auto current_index = token_pipe.get_current_index();

        while (current_index < tokens_count) {
            if (auto s = parse_statement()) {
                mod.statements.push_back(std::move(s));
            }

            current_index = token_pipe.get_current_index();
        }

        return mod;
    }
}
