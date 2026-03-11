#include "Parser.h"
#include "Statement.h"
#include "Token.h"
#include "TokenPipe.h"

#include <utility>
#include <vector>

namespace Wasp {
Parser::Parser() { register_all_parselets(); }

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
} // namespace Wasp
