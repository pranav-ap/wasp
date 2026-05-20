#include "test_utils.h"
#include "AST.h"
#include "Lexer.h"
#include "Parser.h"
#include "StupidLiar.h"


#include <string>

Wasp::StatementVector parse(const std::string& code) {
    Wasp::Lexer lexer;
    Wasp::Parser parser;
    Wasp::StupidLiar liar;

    auto tokens = lexer.run(code);
    auto block = parser.run(tokens);
    // block = liar.run(block);

    return block;
}
