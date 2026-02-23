#include "token.h"
#include "lexer.h"
#include "parser.h"
#include "test_utils.h"
#include <gtest/gtest.h>


TEST(ParseLiterals, Number) {
    auto mod = parse("2");
    ASSERT_EQ(mod.statements.size(), 1);

    auto& stmt = check<Wasp::ExpressionStatement>(mod.statements[0]);
    auto& value = check<int>(stmt.expression);
    EXPECT_EQ(value, 2);
}

TEST(ParseLiterals, Addition) {
    auto mod = parse("1 + 2");
    ASSERT_EQ(mod.statements.size(), 1);

    auto& stmt = check<Wasp::ExpressionStatement>(mod.statements[0]);
    auto& op = check<Wasp::Infix>(stmt.expression);
    
    EXPECT_EQ(check<int>(op.left), 1);
    EXPECT_EQ(check<int>(op.right), 2);
}

TEST(ParseLiterals, List) {
    auto mod = parse("[1, 2, 3]");
    auto& stmt = check<Wasp::ExpressionStatement>(mod.statements[0]);
    check_sequence<Wasp::ListLiteral>(stmt.expression, {1, 2, 3});
}

TEST(ParseLiterals, EmptyList) {
    auto mod = parse("[]");
    auto& stmt = check<Wasp::ExpressionStatement>(mod.statements[0]);
    check_sequence<Wasp::ListLiteral>(stmt.expression);
}

TEST(ParseLiterals, MapLiteral) {
    auto mod = parse("{1 => 1, 2 => '2', 3 => 3.0}");
    ASSERT_EQ(mod.statements.size(), 1);

    auto& stmt = check<Wasp::ExpressionStatement>(mod.statements[0]);
    auto& m = check<Wasp::MapLiteral>(stmt.expression);
    ASSERT_EQ(m.pairs.size(), 3);    
}

TEST(ParseLiterals, EmptyMapLiteral) {
    auto mod = parse("{=>}");
    ASSERT_EQ(mod.statements.size(), 1);

    auto& stmt = check<Wasp::ExpressionStatement>(mod.statements[0]);
    auto& m = check<Wasp::MapLiteral>(stmt.expression);
    ASSERT_EQ(m.pairs.size(), 0);
}

TEST(ParseLiterals, CurlyBracyEmpty) {
    auto mod = parse("{}");
    ASSERT_EQ(mod.statements.size(), 1);

    auto& stmt = check<Wasp::ExpressionStatement>(mod.statements[0]);
    auto& m = check<Wasp::SetLiteral>(stmt.expression);
    ASSERT_EQ(m.expressions.size(), 0);
}
