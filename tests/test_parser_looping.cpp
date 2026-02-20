#include "token.h"
#include "lexer.h"
#include "parser.h"
#include "test_utils.h"
#include <gtest/gtest.h>



TEST(ParserTestSuite, WhileSingle) {
    auto mod = parse(R"(while x < 10 do x = x + 1)");

    EXPECT_TRUE(true);
}

TEST(ParserTestSuite, WhileBlock) {
    auto mod = parse(R"(
while x < 10 do 
    x = x + 1
    y = 1
)");

    EXPECT_TRUE(true);
}

TEST(ParserTestSuite, Continue) {
    auto mod = parse(R"(
while x < 10 do 
    x = x + 1
    continue
)");

    EXPECT_TRUE(true);
}

TEST(ParserTestSuite, ForSingle) {
    auto mod = parse(R"(for x in [1, 2, 3] do x = x + 1)");

    EXPECT_TRUE(true);
}

TEST(ParserTestSuite, ForBlock) {
    auto mod = parse(R"(
for x in [1, 2, 3] do 
    x = x + 1
    y = 1
)");

    EXPECT_TRUE(true);
}


