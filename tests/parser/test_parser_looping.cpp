#include "Expression.h"
#include "Statement.h"
#include "Token.h"
#include "test_utils.h"

#include <gtest/gtest.h>

TEST(ParseLooping, WhileSingle)
{
    auto block = parse(R"(while x < 10 do x = x + 1)");

    auto& stmt = check<Wasp::SimpleLoop>(block[0]);

    {
        auto& condInfix = check<Wasp::Infix>(stmt.condition);
        auto& left = check<Wasp::Identifier>(condInfix.left);
        EXPECT_EQ(left.name, "x");

        // Check for IntegerLiteral instead of raw int
        auto& right = check<Wasp::IntegerLiteral>(condInfix.right);
        EXPECT_EQ(right.value, 10);
        EXPECT_EQ(condInfix.op.type, Wasp::TokenType::LESSER_THAN);
    }

    {
        auto& body = check<Wasp::ExpressionStatement>(stmt.body[0]);
        auto& assign = check<Wasp::Assignment>(body.expression);

        {
            auto& lhs = check<Wasp::Identifier>(assign.lhs);
            EXPECT_EQ(lhs.name, "x");
        }

        {
            auto& rhs = check<Wasp::Infix>(assign.rhs);
            EXPECT_EQ(rhs.op.type, Wasp::TokenType::PLUS);

            {
                auto& inner_left = check<Wasp::Identifier>(rhs.left);
                EXPECT_EQ(inner_left.name, "x");

                // Check for IntegerLiteral instead of raw int
                auto& inner_right = check<Wasp::IntegerLiteral>(rhs.right);
                EXPECT_EQ(inner_right.value, 1);
            }
        }
    }
}

TEST(ParseLooping, WhileBlock)
{
    auto block = parse(R"(
while x < 10 do
    x = x + 1
    y = 1
)");

    ASSERT_EQ(block.size(), 1);

    auto& stmt = check<Wasp::SimpleLoop>(block[0]);
    auto& body = stmt.body;
    ASSERT_EQ(body.size(), 2);

    {
        auto& exprStmt = check<Wasp::ExpressionStatement>(body[0]);
        auto& assign = check<Wasp::Assignment>(exprStmt.expression);
    }

    {
        auto& exprStmt = check<Wasp::ExpressionStatement>(body[1]);
        auto& assign = check<Wasp::Assignment>(exprStmt.expression);
    }
}

TEST(ParseLooping, Continue)
{
    auto block = parse(R"(
while x < 10 do
    x = x + 1
    continue
)");

    auto& stmt = check<Wasp::SimpleLoop>(block[0]);
    auto& body = stmt.body;
    ASSERT_EQ(body.size(), 2);

    auto& exprStmt = check<Wasp::ExpressionStatement>(body[0]);
    auto& assign = check<Wasp::Assignment>(exprStmt.expression);

    auto& ctrlStmt = check<Wasp::LoopControl>(body[1]);
    EXPECT_EQ(ctrlStmt.type, Wasp::TokenType::CONTINUE);
}

TEST(ParseLooping, ContinueWithExpression)
{
    auto block = parse(R"(
while x < 10 do
    x = x + 1
    continue x
)");

    auto& stmt = check<Wasp::SimpleLoop>(block[0]);
    auto& body = stmt.body;
    ASSERT_EQ(body.size(), 2);

    auto& exprStmt = check<Wasp::ExpressionStatement>(body[0]);
    auto& assign = check<Wasp::Assignment>(exprStmt.expression);

    auto& ctrlStmt = check<Wasp::LoopControl>(body[1]);
    EXPECT_EQ(ctrlStmt.type, Wasp::TokenType::CONTINUE);
}
