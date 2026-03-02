#include "Token.h"
#include "lexer.h"
#include "parser.h"
#include "test_utils.h"
#include <gtest/gtest.h>

TEST(ParseExpressions, Dot)
{
    auto mod = parse(".");
    ASSERT_EQ(mod.statements.size(), 1);

    auto &stmt = check<Wasp::ExpressionStatement>(mod.statements[0]);
    ASSERT_NE(stmt.expression, nullptr);

    // This will successfully verify the node is a DotLiteral
    auto &dot = check<Wasp::DotLiteral>(stmt.expression);
}

TEST(ParseExpressions, RangeSimpleExclusive)
{
    auto mod = parse("1..<10");

    auto &stmt = check<Wasp::ExpressionStatement>(mod.statements[0]);
    auto &range = check<Wasp::RangeLiteral>(stmt.expression);
    EXPECT_FALSE(range.is_inclusive); // ..< is exclusive

    auto &start = check<int>(range.start);
    EXPECT_EQ(start, 1);

    auto &end = check<int>(range.end);
    EXPECT_EQ(end, 10);

    EXPECT_EQ(range.step, nullptr);
}

TEST(ParseExpressions, RangeWithStep)
{
    auto mod = parse("1..<10 step 2");

    auto &stmt = check<Wasp::ExpressionStatement>(mod.statements[0]);
    auto &range = check<Wasp::RangeLiteral>(stmt.expression);
    EXPECT_FALSE(range.is_inclusive); // ..< is exclusive

    auto &start = check<int>(range.start);
    EXPECT_EQ(start, 1);

    auto &end = check<int>(range.end);
    EXPECT_EQ(end, 10);

    auto &step = check<int>(range.step);
    EXPECT_EQ(step, 2);
}

TEST(ParseExpressions, RangeWithoutEnd)
{
    auto mod = parse("1..<");

    auto &stmt = check<Wasp::ExpressionStatement>(mod.statements[0]);
    auto &range = check<Wasp::RangeLiteral>(stmt.expression);
    EXPECT_FALSE(range.is_inclusive); // FIXED: ..< is exclusive

    auto &start = check<int>(range.start);
    EXPECT_EQ(start, 1);

    EXPECT_EQ(range.end, nullptr);
    EXPECT_EQ(range.step, nullptr);
}

TEST(ParseExpressions, RangeWithoutEndWithStep)
{
    auto mod = parse("1..= step 2");

    auto &stmt = check<Wasp::ExpressionStatement>(mod.statements[0]);
    auto &range = check<Wasp::RangeLiteral>(stmt.expression);
    EXPECT_TRUE(range.is_inclusive);

    auto &start = check<int>(range.start);
    EXPECT_EQ(start, 1);

    EXPECT_EQ(range.end, nullptr);

    auto &step = check<int>(range.step);
    EXPECT_EQ(step, 2);
}
