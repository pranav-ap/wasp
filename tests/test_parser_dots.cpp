#include "token.h"
#include "lexer.h"
#include "parser.h"
#include "test_utils.h"
#include <gtest/gtest.h>


TEST(ParseExpressions, RangeSimpleExclusive) {
    auto mod = parse("1..10");

    auto& stmt = check<Wasp::ExpressionStatement>(mod.statements[0]);
    auto& range = check<Wasp::RangeLiteral>(stmt.expression);
    EXPECT_FALSE(range.is_inclusive);

    auto& start = check<int>(range.start);
    EXPECT_EQ(start, 1);

    auto& end = check<int>(range.end);
    EXPECT_EQ(end, 10);

    EXPECT_EQ(range.step, nullptr);
} 

TEST(ParseExpressions, RangeWithStep) {
    auto mod = parse("1..10:2");

    auto& stmt = check<Wasp::ExpressionStatement>(mod.statements[0]);
    auto& range = check<Wasp::RangeLiteral>(stmt.expression);
    EXPECT_FALSE(range.is_inclusive);

    auto& start = check<int>(range.start);
    EXPECT_EQ(start, 1);

    auto& end = check<int>(range.end);
    EXPECT_EQ(end, 10);

    auto& step = check<int>(range.step);
    EXPECT_EQ(step, 2);
}

TEST(ParseExpressions, RangeWithoutEnd) {
    auto mod = parse("1...");
    
    auto& stmt = check<Wasp::ExpressionStatement>(mod.statements[0]);
    auto& range = check<Wasp::RangeLiteral>(stmt.expression);
    EXPECT_TRUE(range.is_inclusive);

    auto& start = check<int>(range.start);
    EXPECT_EQ(start, 1);

    EXPECT_EQ(range.end, nullptr);
    EXPECT_EQ(range.step, nullptr);
}

TEST(ParseExpressions, RangeWithoutEndWithStep) {
    auto mod = parse("1..:2");
    
    auto& stmt = check<Wasp::ExpressionStatement>(mod.statements[0]);
    auto& range = check<Wasp::RangeLiteral>(stmt.expression);
    EXPECT_FALSE(range.is_inclusive);

    auto& start = check<int>(range.start);
    EXPECT_EQ(start, 1);

    EXPECT_EQ(range.end, nullptr);

    auto& step = check<int>(range.step);
    EXPECT_EQ(step, 2);
}

TEST(ParseExpressions, Dot) {
    auto mod = parse(".");
    ASSERT_EQ(mod.statements.size(), 1);

    auto& stmt = check<Wasp::ExpressionStatement>(mod.statements[0]);
    ASSERT_NE(stmt.expression, nullptr);

    auto& dot = check<Wasp::DotLiteral>(stmt.expression);
}

TEST(ParseExpressions, DotDot) {
    auto mod = parse("..");
    ASSERT_EQ(mod.statements.size(), 1);

    auto& stmt = check<Wasp::ExpressionStatement>(mod.statements[0]);
    ASSERT_NE(stmt.expression, nullptr);

    auto& dotDot = check<Wasp::DotDotLiteral>(stmt.expression);
}

TEST(ParseExpressions, DotDotDot) {
    auto mod = parse("...");
    ASSERT_EQ(mod.statements.size(), 1);

    auto& stmt = check<Wasp::ExpressionStatement>(mod.statements[0]);
    ASSERT_NE(stmt.expression, nullptr);

    auto& dotDotDot = check<Wasp::DotDotDotLiteral>(stmt.expression);
}
