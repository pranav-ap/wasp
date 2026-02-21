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

using std::move;
using std::cout;
using std::optional;
using std::endl;
using std::make_pair;
using std::make_shared;

namespace Wasp {
    Parser::Parser() {
        register_all_parselets();
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
