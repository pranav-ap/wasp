#include <gtest/gtest.h>
#include "lexer.h" 


TEST(LexerTestSuite, BasicTokenTest) {
    EXPECT_TRUE(true); 
}

TEST(LexerTestSuite, StringCheck) {
    std::string test_val = "wasp";
    EXPECT_EQ(test_val, "wasp");
}