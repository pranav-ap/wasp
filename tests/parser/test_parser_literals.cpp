#include "Expression.h"
#include "Statement.h"
#include "Token.h"
#include "test_utils.h"
#include <gtest/gtest.h>


TEST(ParseLiterals, Number) {
    auto block = parse("2");
    ASSERT_EQ(block.size(), 1);

    auto& stmt = check<Wasp::ExpressionStatement>(block[0]);
    auto& value = check<int>(stmt.expression);
    EXPECT_EQ(value, 2);
}

TEST(ParseLiterals, NegativeNumber)
{
    auto block = parse("-2");
    ASSERT_EQ(block.size(), 1);

    auto& stmt = check<Wasp::ExpressionStatement>(block[0]);
    auto& prefix = check<Wasp::Prefix>(stmt.expression);
    EXPECT_EQ(prefix.op.type, Wasp::TokenType::MINUS);

    auto& value = check<int>(prefix.operand);
    EXPECT_EQ(value, 2);
}

TEST(ParseLiterals, Addition) {
    auto block = parse("1 + 2");
    ASSERT_EQ(block.size(), 1);

    auto& stmt = check<Wasp::ExpressionStatement>(block[0]);
    auto& op = check<Wasp::Infix>(stmt.expression);

    EXPECT_EQ(check<int>(op.left), 1);
    EXPECT_EQ(check<int>(op.right), 2);
}

TEST(ParseLiterals, List) {
    auto block = parse("[1, 2, 3]");
    auto& stmt = check<Wasp::ExpressionStatement>(block[0]);
    check_sequence<Wasp::ListLiteral>(stmt.expression, {1, 2, 3});
}

TEST(ParseLiterals, EmptyList) {
    auto block = parse("[]");
    auto& stmt = check<Wasp::ExpressionStatement>(block[0]);
    check_sequence<Wasp::ListLiteral>(stmt.expression);
}

TEST(ParseLiterals, MapLiteral) {
    auto block = parse("{1 => 1, 2 => '2', 3 => 3.0}");
    ASSERT_EQ(block.size(), 1);

    auto& stmt = check<Wasp::ExpressionStatement>(block[0]);
    auto& m = check<Wasp::MapLiteral>(stmt.expression);
    ASSERT_EQ(m.pairs.size(), 3);
}

TEST(ParseLiterals, EmptyMapLiteral) {
    auto block = parse("{=>}");
    ASSERT_EQ(block.size(), 1);

    auto& stmt = check<Wasp::ExpressionStatement>(block[0]);
    auto& m = check<Wasp::MapLiteral>(stmt.expression);
    ASSERT_EQ(m.pairs.size(), 0);
}

TEST(ParseLiterals, CurlyBracyEmpty) {
    auto block = parse("{}");
    ASSERT_EQ(block.size(), 1);

    auto& stmt = check<Wasp::ExpressionStatement>(block[0]);
    auto& m = check<Wasp::SetLiteral>(stmt.expression);
    ASSERT_EQ(m.expressions.size(), 0);
}
