#include "Parser.h"

#include <memory>
#include <utility>

using std::make_pair;

namespace Wasp {
    void Parser::register_parselet(TokenType token_type, IPrefixParselet_ptr parselet) {
        prefix_parselets.insert(make_pair(token_type, parselet));
    }

    void Parser::register_parselet(TokenType token_type, IInfixParselet_ptr parselet) {
        infix_parselets.insert(make_pair(token_type, parselet));
    }

    void Parser::register_prefix(TokenType token_type, Precedence precedence) {
        prefix_parselets.insert(
            make_pair(token_type, std::make_shared<PrefixOperatorParselet>(static_cast<int>(precedence))));
    }

    void Parser::register_infix_left(TokenType token_type, Precedence precedence) {
        register_parselet(token_type, std::make_shared<InfixOperatorParselet>(static_cast<int>(precedence), false));
    }

    void Parser::register_infix_right(TokenType token_type, Precedence precedence) {
        register_parselet(token_type, std::make_shared<InfixOperatorParselet>(static_cast<int>(precedence), true));
    }
}
