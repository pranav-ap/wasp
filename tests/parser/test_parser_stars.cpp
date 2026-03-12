#include "Token.h"
#include "Lexer.h"
#include "Parser.h"
#include "test_utils.h"
#include <gtest/gtest.h>


TEST(ParseStarExpressions, Star) {
    auto block = parse("*b");
    ASSERT_EQ(block.size(), 1);

    auto& stmt = check<Wasp::ExpressionStatement>(block[0]);
    auto& star = check<Wasp::Prefix>(stmt.expression);
    EXPECT_EQ(star.op.type, Wasp::TokenType::STAR);

    auto& id = check<Wasp::Identifier>(star.operand);
    EXPECT_EQ(id.name, "b");
}

TEST(ParseStarExpressions, StarGather) {
    auto block = parse("[a, *b, c] = [1, 2, 3, 4, 5]");
    ASSERT_EQ(block.size(), 1);

    auto& stmt = check<Wasp::ExpressionStatement>(block[0]);
    auto& assign = check<Wasp::UntypedAssignment>(stmt.expression);
    {
        auto& lhs = check<Wasp::ListLiteral>(assign.lhs_expression);
        ASSERT_EQ(lhs.expressions.size(), 3);
    
        {
            auto& starGather = check<Wasp::Prefix>(lhs.expressions[1]);
            EXPECT_EQ(starGather.op.type, Wasp::TokenType::STAR);
            auto& gatherId = check<Wasp::Identifier>(starGather.operand);
            EXPECT_EQ(gatherId.name, "b");
        }
    }
    
    {
        auto& rhs = check<Wasp::ListLiteral>(assign.rhs_expression);
        ASSERT_EQ(rhs.expressions.size(), 5);
    }
}


TEST(ParseStarExpressions, StarGatherAndSpread) {
    auto block = parse("[a, *b, c] = *five_nums");
    ASSERT_EQ(block.size(), 1);

    auto& stmt = check<Wasp::ExpressionStatement>(block[0]);
    auto& assign = check<Wasp::UntypedAssignment>(stmt.expression);
    
    {
        auto& lhs = check<Wasp::ListLiteral>(assign.lhs_expression);
        ASSERT_EQ(lhs.expressions.size(), 3);
    
        {
            auto& starGather = check<Wasp::Prefix>(lhs.expressions[1]);
            auto& gatherId = check<Wasp::Identifier>(starGather.operand);
            EXPECT_EQ(gatherId.name, "b");
        }
    }
    
    {
        auto& rhs = check<Wasp::Prefix>(assign.rhs_expression);
        EXPECT_EQ(rhs.op.type, Wasp::TokenType::STAR);

        auto& spreadId = check<Wasp::Identifier>(rhs.operand);
        EXPECT_EQ(spreadId.name, "five_nums");
    }
}
