#include "test_utils.h"
#include "lexer.h"
#include "parser.h"

Wasp::Module parse(const std::string& code) {
    Wasp::Lexer lexer;
    Wasp::Parser parser;

    auto tokens = lexer.run(code);
    auto mod = parser.run(tokens);

    return mod;
}


