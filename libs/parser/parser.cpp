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
    Parser::Parser() {
        register_parselet(TokenType::IDENTIFIER, make_shared<IdentifierParselet>());
        register_parselet(TokenType::STRING_LITERAL, make_shared<LiteralParselet>());
        register_parselet(TokenType::NUMBER_LITERAL, make_shared<LiteralParselet>());
        register_parselet(TokenType::TRUE_KEYWORD, make_shared<LiteralParselet>());
        register_parselet(TokenType::FALSE_KEYWORD, make_shared<LiteralParselet>());
        register_parselet(TokenType::NONE, make_shared<LiteralParselet>());

        register_parselet(TokenType::OPEN_SQUARE_BRACKET, make_shared<ListParselet>());
        register_parselet(TokenType::OPEN_CURLY_BRACE, make_shared<CurlyBraceParselet>());
        
        register_parselet(TokenType::OPEN_PARENTHESIS, make_shared<ParenthesisParselet>());
        register_parselet(TokenType::OPEN_PARENTHESIS, make_shared<CallParselet>());
               
        register_prefix(TokenType::PLUS, Precedence::PREFIX);
        register_prefix(TokenType::MINUS, Precedence::PREFIX);
        register_prefix(TokenType::NOT, Precedence::PREFIX);

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
        register_infix_left(TokenType::IS, Precedence::COMPARISON);
        register_infix_left(TokenType::AND, Precedence::AND);
        register_infix_left(TokenType::OR, Precedence::OR);

        register_infix_right(TokenType::POWER, Precedence::EXPONENT);

        register_infix_right(TokenType::EQUAL, Precedence::ASSIGNMENT);
        register_parselet(TokenType::EQUAL, make_shared<AssignmentParselet>());

        register_parselet(TokenType::COLON, make_shared<TypePatternParselet>());
        register_parselet(TokenType::IF, make_shared<TernaryConditionParselet>());
        
	    register_infix_left(TokenType::IN_KEYWORD, Precedence::COMPARISON);

        register_infix_left(TokenType::DOT, Precedence::MEMBER_ACCESS);
        register_parselet(TokenType::DOT, make_shared<PlaceholderDotParselet>());

        auto range_pre = make_shared<PrefixRangeParselet>();
        register_parselet(TokenType::DOT_DOT, range_pre);
        register_parselet(TokenType::DOT_DOT_DOT, range_pre);

        auto range_inf = make_shared<InfixRangeParselet>();
        register_parselet(TokenType::DOT_DOT, range_inf);
        register_parselet(TokenType::DOT_DOT_DOT, range_inf);

        register_infix_left(TokenType::TILDE, Precedence::PIPE);

        register_parselet(TokenType::STAR, make_shared<StarGatherSpreadParselet>());
    }

    Module Parser::run(const std::vector<Token> &tokens) {
        token_pipe = TokenPipe(tokens);
        Module mod;

        const auto tokens_count = token_pipe.get_size();
        auto current_index = token_pipe.get_current_index();

        while (current_index < tokens_count) {
            if (auto s = parse_statement(0)) {
                mod.statements.push_back(std::move(s));
            }

            current_index = token_pipe.get_current_index();
        }

        return mod;
    }
}
