#include "test_utils.h"
#include "AST.h"
#include "Lexer.h"
#include "Liar.h"
#include "Parser.h"

#include <string>

Wasp::StatementVector parse(const std::string& code) {
    Wasp::Lexer lexer;
    Wasp::Parser parser;
    Wasp::Liar liar;

    auto tokens = lexer.run(code);
    auto block = parser.run(tokens);
    // block = liar.run(block);

    return block;
}
