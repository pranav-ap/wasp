#include "Lexer.h"
#include <gtest/gtest.h>

TEST(LexerTestSuite, SanityTest) {
    EXPECT_TRUE(true);
}


TEST(LexerTestSuite, TokenizeSimpleCode) {
    Wasp::Lexer lexer;
    std::string code = "let x = 10";
    auto tokens = lexer.run(code); // retuns space as well

    EXPECT_EQ(tokens.size(), 9); // let, space, x, space, =, space, 10
    EXPECT_EQ(tokens[0].type, Wasp::TokenType::LET);
    EXPECT_EQ(tokens[1].type, Wasp::TokenType::SPACE);
    EXPECT_EQ(tokens[2].type, Wasp::TokenType::IDENTIFIER);
    EXPECT_EQ(tokens[3].type, Wasp::TokenType::SPACE);
    EXPECT_EQ(tokens[4].type, Wasp::TokenType::EQUAL);
    EXPECT_EQ(tokens[5].type, Wasp::TokenType::SPACE);
    EXPECT_EQ(tokens[6].type, Wasp::TokenType::NUMBER_LITERAL);
    EXPECT_EQ(tokens[6].lexeme, "10");
    EXPECT_EQ(tokens[7].type, Wasp::TokenType::EOL);
    EXPECT_EQ(tokens[8].type, Wasp::TokenType::END_OF_FILE);
}
