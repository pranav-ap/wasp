#include "test_utils.h"
#include "Lexer.h"
#include "Parser.h"
#include "Statement.h"


#include <string>

Wasp::Block parse(const std::string& code)
{
    Wasp::Lexer lexer;
    Wasp::Parser parser;

    auto tokens = lexer.run(code);
    auto block = parser.run(tokens);

    return block;
}
