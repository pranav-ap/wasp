#include "Expression.h"
#include "Statement.h"
#include "test_utils.h"
#include <gtest/gtest.h>

TEST(ParseExpressions, Dot)
{
    auto block = parse(".");
    ASSERT_EQ(block.size(), 1);

    auto& stmt = check<Wasp::ExpressionStatement>(block[0]);
    ASSERT_NE(stmt.expression, nullptr);

    auto &dot = check<Wasp::DotLiteral>(stmt.expression);
}

TEST(ParseExpressions, RangeSimpleExclusive)
{
    auto block = parse("1..<10");

    auto& stmt = check<Wasp::ExpressionStatement>(block[0]);
    auto &range = check<Wasp::RangeLiteral>(stmt.expression);
    EXPECT_FALSE(range.is_inclusive);

    auto& start = check<Wasp::IntegerLiteral>(range.start);
    EXPECT_EQ(start.value, 1);

    auto& end = check<Wasp::IntegerLiteral>(range.end);
    EXPECT_EQ(end.value, 10);

    EXPECT_EQ(range.step, nullptr);
}

TEST(ParseExpressions, RangeWithStep)
{
    auto block = parse("1..<10 step 2");

    auto& stmt = check<Wasp::ExpressionStatement>(block[0]);
    auto &range = check<Wasp::RangeLiteral>(stmt.expression);
    EXPECT_FALSE(range.is_inclusive);

    auto& start = check<Wasp::IntegerLiteral>(range.start);
    EXPECT_EQ(start.value, 1);

    auto& end = check<Wasp::IntegerLiteral>(range.end);
    EXPECT_EQ(end.value, 10);

    auto& step = check<Wasp::IntegerLiteral>(range.step);
    EXPECT_EQ(step.value, 2);
}

TEST(ParseExpressions, RangeWithoutEnd)
{
    auto block = parse("1..<");

    auto& stmt = check<Wasp::ExpressionStatement>(block[0]);
    auto &range = check<Wasp::RangeLiteral>(stmt.expression);
    EXPECT_FALSE(range.is_inclusive);

    auto& start = check<Wasp::IntegerLiteral>(range.start);
    EXPECT_EQ(start.value, 1);

    EXPECT_EQ(range.end, nullptr);
    EXPECT_EQ(range.step, nullptr);
}

TEST(ParseExpressions, RangeWithoutEndWithStep)
{
    auto block = parse("1..= step 2");

    auto& stmt = check<Wasp::ExpressionStatement>(block[0]);
    auto &range = check<Wasp::RangeLiteral>(stmt.expression);
    EXPECT_TRUE(range.is_inclusive);

    auto& start = check<Wasp::IntegerLiteral>(range.start);
    EXPECT_EQ(start.value, 1);

    EXPECT_EQ(range.end, nullptr);

    auto& step = check<Wasp::IntegerLiteral>(range.step);
    EXPECT_EQ(step.value, 2);
}
