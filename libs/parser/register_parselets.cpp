#include "Parser.h"

#include <memory>
#include <utility>

using std::make_pair;
using std::make_shared;

namespace Wasp
{
    void Parser::register_all_parselets()
    {
        register_prefix(TokenType::PLUS, Precedence::PREFIX);
        register_prefix(TokenType::MINUS, Precedence::PREFIX);
        register_prefix(TokenType::NOT, Precedence::PREFIX);
        register_prefix(TokenType::STAR, Precedence::PREFIX);

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
        register_infix_left(TokenType::ROCKET, Precedence::COMPARISON);
        register_infix_left(TokenType::ZIP, Precedence::COMPARISON);

        register_infix_left(TokenType::IN_KEYWORD, Precedence::COMPARISON);
        register_infix_left(TokenType::IS, Precedence::COMPARISON);

        register_infix_left(TokenType::AND, Precedence::AND);
        register_infix_left(TokenType::OR, Precedence::OR);

        register_infix_right(TokenType::POWER, Precedence::EXPONENT);

        register_infix_left(TokenType::TILDE, Precedence::PIPE);
        register_infix_left(TokenType::DOT, Precedence::MEMBER_ACCESS);

        register_parselet(TokenType::IDENTIFIER, make_shared<IdentifierParselet>());
        register_parselet(TokenType::MY, make_shared<IdentifierParselet>());
        register_parselet(TokenType::OUR, make_shared<IdentifierParselet>());

        register_parselet(TokenType::STRING_LITERAL, make_shared<LiteralParselet>());
        register_parselet(TokenType::NUMBER_LITERAL, make_shared<LiteralParselet>());
        register_parselet(TokenType::TRUE_KEYWORD, make_shared<LiteralParselet>());
        register_parselet(TokenType::FALSE_KEYWORD, make_shared<LiteralParselet>());
        register_parselet(TokenType::NONE, make_shared<LiteralParselet>());

        register_parselet(TokenType::OPEN_SQUARE_BRACKET, make_shared<SquareBracketParselet>());
        register_parselet(TokenType::OPEN_CURLY_BRACE, make_shared<CurlyBraceParselet>());
        register_parselet(TokenType::OPEN_PARENTHESIS, make_shared<ParenthesisParselet>());
        register_parselet(TokenType::OPEN_PARENTHESIS, make_shared<CallParselet>());

        register_parselet(TokenType::EQUAL, make_shared<AssignmentParselet>());
        register_parselet(TokenType::COLON, make_shared<TypePatternParselet>());
        register_parselet(TokenType::IF, make_shared<TernaryConditionParselet>());

        register_parselet(TokenType::DOT, make_shared<PlaceholderDotParselet>());
        register_parselet(TokenType::DOT_DOT_LESS, std::make_shared<PrefixRangeParselet>(false));
        register_parselet(TokenType::DOT_DOT_EQUAL, std::make_shared<PrefixRangeParselet>(true));
        register_parselet(TokenType::DOT_DOT_LESS, std::make_shared<InfixRangeParselet>(false));
        register_parselet(TokenType::DOT_DOT_EQUAL, std::make_shared<InfixRangeParselet>(true));
    }

    void Parser::register_parselet(TokenType token_type, IPrefixParselet_ptr parselet)
    {
        prefix_parselets.insert(make_pair(token_type, parselet));
    }

    void Parser::register_parselet(TokenType token_type, IInfixParselet_ptr parselet)
    {
        infix_parselets.insert(make_pair(token_type, parselet));
    }

    void Parser::register_prefix(TokenType token_type, Precedence precedence)
    {
        prefix_parselets.insert(
            make_pair(token_type, std::make_shared<PrefixOperatorParselet>(static_cast<int>(precedence))));
    }

    void Parser::register_infix_left(TokenType token_type, Precedence precedence)
    {
        register_parselet(token_type, std::make_shared<InfixOperatorParselet>(static_cast<int>(precedence), false));
    }

    void Parser::register_infix_right(TokenType token_type, Precedence precedence)
    {
        register_parselet(token_type, std::make_shared<InfixOperatorParselet>(static_cast<int>(precedence), true));
    }
}
