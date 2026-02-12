#include "lexer.h"
#include "parser.h"

#include <gtest/gtest.h>
#include <string>


TEST(ParserTestSuite, ParseNumber) {
    Wasp::Lexer lexer;
    Wasp::Parser parser;

    const std::string code = R"(
2
)";

    const auto tokens = lexer.run(code);
    auto module = parser.run(tokens);

    EXPECT_TRUE(true);
}

TEST(ParserTestSuite, ParsePrefixNumber) {
    Wasp::Lexer lexer;
    Wasp::Parser parser;

    const std::string code = R"(
+2
)";

    const auto tokens = lexer.run(code);
    auto module = parser.run(tokens);

    EXPECT_TRUE(true);
}

TEST(ParserTestSuite, ParseAddition) {
    Wasp::Lexer lexer;
    Wasp::Parser parser;

    const std::string code = R"(
1 + 2
)";

    const auto tokens = lexer.run(code);
    auto module = parser.run(tokens);

    EXPECT_TRUE(true);
}

