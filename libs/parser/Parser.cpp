#include "Parser.h"
#include "AST.h"
#include "Token.h"
#include "TokenPipe.h"

#include <utility>
#include <vector>

namespace Wasp
{

Parser::Parser() { register_all_parselets(); }

StatementVector Parser::run(const std::vector<Token>& tokens) {
    token_pipe = TokenPipe(tokens);
    StatementVector block;

    const int tokens_count = token_pipe.get_size();
    auto current_index = token_pipe.get_current_index();

    while (current_index < tokens_count) {
        if (auto s = parse_statement(0)) {
            block.push_back(std::move(s));
        }

        current_index = token_pipe.get_current_index();
    }

    return block;
}
} // namespace Wasp
