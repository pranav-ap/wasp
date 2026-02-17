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
using std::make_shared;

namespace Wasp {
    Expression_ptr Parser::parse_expression() {
        return parse_expression(0);
    }

    Expression_ptr Parser::parse_expression(const int precedence) {
        auto token = token_pipe.current_in_line();
        RETURN_IF_NULLOPT(token);

        if (token.value().type == TokenType::LET) {
            token_pipe.advance_pointer();
            Expression_ptr expr = parse_expression();
            return MAKE_EXPRESSION(VariableDefinitionExpression(expr, true));
        }
        
        if (token.value().type == TokenType::CONST_KEYWORD) {
            token_pipe.advance_pointer();
            Expression_ptr expr = parse_expression();
            return MAKE_EXPRESSION(VariableDefinitionExpression(expr, false));
        }

        const IPrefixParselet_ptr prefix_parselet = prefix_parselets.at(token.value().type);
        EXIT_IF_NULLPTR(prefix_parselet);
        Expression_ptr left = prefix_parselet->parse(*this, token.value());
        EXIT_IF_NULLPTR(left);

        while (precedence < get_next_operator_precedence()) {
            token = token_pipe.current_in_line();
            EXIT_IF_NULLOPT(token);

            token_pipe.advance_pointer();

            const IInfixParselet_ptr infix_parselet = infix_parselets.at(token.value().type);
            left = infix_parselet->parse(*this, left, token.value());
        }

        return left;
    }

    ExpressionVector Parser::parse_expressions() {
        ExpressionVector elements;

        while (auto element = parse_expression()) {
            elements.push_back(move(element));

            // If there's a comma, consume it and continue the loop
            if (const auto token = token_pipe.later(); 
                token.has_value() && token->type == TokenType::COMMA) {
                token_pipe.advance_pointer();
                continue; 
            }

            // No comma? We're done with the list.
            break;
        }

        return elements;
    }

    int Parser::get_next_operator_precedence() {
        if (const auto token = token_pipe.current_in_line();
            token.has_value() && infix_parselets.contains(token.value().type)) {
            const IInfixParselet_ptr infix_parselet = infix_parselets.at(token.value().type);
            return infix_parselet->get_precedence();
        }

        return 0;
    }
}
