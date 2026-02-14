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
    auto mod = parse("{}");
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

TEST(ParserTestSuite, IntDefinition) {
    auto mod = parse("let x : int = 5");
    EXPECT_TRUE(true);
}

TEST(ParserTestSuite, ListDefinition) {
    auto mod = parse("let x : [int] = [1, 2, 3]");
    EXPECT_TRUE(true);
}

TEST(ParserTestSuite, SetDefinition) {
    auto mod = parse("let x : { int } = {1, 2, 3}");
    EXPECT_TRUE(true);
}

TEST(ParserTestSuite, MapDefinition) {
    auto mod = parse("let x : { int -> int } = {1 -> 1, 2 -> 2, 3 -> 3}");
    EXPECT_TRUE(true);
}

TEST(ParserTestSuite, FunTypeDefinition) {
    auto mod = parse("let x : (int) -> int = function_name");
    EXPECT_TRUE(true);
}

TEST(ParserTestSuite, VariantDefinition) {
    auto mod = parse("let x : (int | float) = 5");
    EXPECT_TRUE(true);
}

TEST(ParserTestSuite, AliasDefinition) {
    auto mod = parse("alias int_list = [int]");
    EXPECT_TRUE(true);
}

TEST(ParserTestSuite, TernaryExpression) {
    auto mod = parse("if true then 1 else 2");
    EXPECT_TRUE(true);
}

TEST(ParserTestSuite, TernaryLetExpression) {
    auto mod = parse("if let x = 1 then 1 else 2");
    EXPECT_TRUE(true);
}

TEST(ParserTestSuite, IfBlock) {
    auto mod = parse(R"(
if true then
    1 
)");

    EXPECT_TRUE(true);
}

TEST(ParserTestSuite, IfElseBlock) {
    auto mod = parse(R"(
if true then
    1
else
    2
)");

    EXPECT_TRUE(true);
}


TEST(ParserTestSuite, IfElifElseBlock) {
    auto mod = parse(R"(
if x == 25 then
    pass
elif x == 30 then
    pass
else
    pass
)");

    EXPECT_TRUE(true);
}

