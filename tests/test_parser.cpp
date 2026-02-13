#include "lexer.h"
#include "parser.h"
#include <gtest/gtest.h>


Wasp::Module parse(const std::string& code) {
    Wasp::Lexer lexer;
    Wasp::Parser parser;

    auto tokens = lexer.run(code);
    auto mod = parser.run(tokens);

    return mod;
}

TEST(ParserTestSuite, Number) {
    auto mod = parse("2");
    EXPECT_TRUE(true); 
}

TEST(ParserTestSuite, Addition) {
    auto mod = parse("1 + 2");
    EXPECT_TRUE(true);
}

TEST(ParserTestSuite, List) {
    auto mod = parse("[1, 2, 3]");
    EXPECT_TRUE(true);
}

TEST(ParserTestSuite, Tuple) {
    auto mod = parse("(1, 2, 3)");
    EXPECT_TRUE(true);
}


TEST(ParserTestSuite, EmptySet) {
    auto mod = parse("{1, 2, 3}");
    EXPECT_TRUE(true);
}


TEST(ParserTestSuite, Set) {
    auto mod = parse("{1, 2, 3}");
    EXPECT_TRUE(true);
}


TEST(ParserTestSuite, EmptyMap) {
    auto mod = parse("{->}");
    EXPECT_TRUE(true);
}

TEST(ParserTestSuite, Map) {
    auto mod = parse("{1 -> 1, 2 -> '2', 3 -> 3.0}");
    EXPECT_TRUE(true);
}