#include "token.h"
#include "lexer.h"
#include "parser.h"
#include "test_utils.h"
#include <gtest/gtest.h>


TEST(ParserTestSuite, Number) {
    auto mod = parse("2");
    ASSERT_EQ(mod.statements.size(), 1);

    auto* exprStmt = std::get_if<Wasp::ExpressionStatement>(&mod.statements[0]->data);
    ASSERT_NE(exprStmt, nullptr) << "Expected an ExpressionStatement";

    ASSERT_TRUE(exprStmt->expression->is<int>()) << "Expected an integer expression";
    EXPECT_EQ(exprStmt->expression->as<int>(), 2);
}


TEST(ParserTestSuite, Addition) {
    auto mod = parse("1 + 2");
    ASSERT_EQ(mod.statements.size(), 1);

    auto* exprStmt = std::get_if<Wasp::ExpressionStatement>(&mod.statements[0]->data);
    ASSERT_NE(exprStmt, nullptr);

    ASSERT_TRUE(exprStmt->expression->is<Wasp::Infix>());
    const auto& infix = exprStmt->expression->as<Wasp::Infix>();

    ASSERT_TRUE(infix.left->is<int>());
    EXPECT_EQ(infix.left->as<int>(), 1);

    EXPECT_EQ(infix.op.type, Wasp::TokenType::PLUS); 

    ASSERT_TRUE(infix.right->is<int>());
    EXPECT_EQ(infix.right->as<int>(), 2);
}

TEST(ParserTestSuite, List) {
    auto mod = parse("[1, 2, 3]");
    ASSERT_EQ(mod.statements.size(), 1);

    auto* exprStmt = std::get_if<Wasp::ExpressionStatement>(&mod.statements[0]->data);
    ASSERT_NE(exprStmt, nullptr);

    ASSERT_TRUE(exprStmt->expression->is<Wasp::ListLiteral>());
    const auto& list = exprStmt->expression->as<Wasp::ListLiteral>();

    ASSERT_EQ(list.expressions.size(), 3);
    ASSERT_TRUE(list.expressions[0]->is<int>());
    EXPECT_EQ(list.expressions[0]->as<int>(), 1);
}

TEST(ParserTestSuite, Tuple) {
    auto mod = parse("(1, 2, 3)");
    ASSERT_EQ(mod.statements.size(), 1);
    
    auto* exprStmt = std::get_if<Wasp::ExpressionStatement>(&mod.statements[0]->data);
    ASSERT_NE(exprStmt, nullptr);

    ASSERT_TRUE(exprStmt->expression->is<Wasp::TupleLiteral>());
    const auto& tuple = exprStmt->expression->as<Wasp::TupleLiteral>();

    ASSERT_EQ(tuple.expressions.size(), 3);
    ASSERT_TRUE(tuple.expressions[0]->is<int>());
    EXPECT_EQ(tuple.expressions[0]->as<int>(), 1);
}


TEST(ParserTestSuite, EmptySet) {
    auto mod = parse("{}");
    ASSERT_EQ(mod.statements.size(), 1);

    auto* exprStmt = std::get_if<Wasp::ExpressionStatement>(&mod.statements[0]->data);
    ASSERT_NE(exprStmt, nullptr);

    ASSERT_TRUE(exprStmt->expression->is<Wasp::SetLiteral>());
    const auto& set = exprStmt->expression->as<Wasp::SetLiteral>();

    ASSERT_EQ(set.expressions.size(), 0);
}


TEST(ParserTestSuite, Set) {
    auto mod = parse("{1, 2, 3}");
    ASSERT_EQ(mod.statements.size(), 1);

    auto* exprStmt = std::get_if<Wasp::ExpressionStatement>(&mod.statements[0]->data);
    ASSERT_NE(exprStmt, nullptr);

    ASSERT_TRUE(exprStmt->expression->is<Wasp::SetLiteral>());
    const auto& set = exprStmt->expression->as<Wasp::SetLiteral>();

    ASSERT_EQ(set.expressions.size(), 3);
    ASSERT_TRUE(set.expressions[0]->is<int>());
    EXPECT_EQ(set.expressions[0]->as<int>(), 1);
}


TEST(ParserTestSuite, EmptyMap) {
    auto mod = parse("{=>}");
    ASSERT_EQ(mod.statements.size(), 1);

    auto* exprStmt = std::get_if<Wasp::ExpressionStatement>(&mod.statements[0]->data);
    ASSERT_NE(exprStmt, nullptr);
    ASSERT_TRUE(exprStmt->expression->is<Wasp::MapLiteral>());
    const auto& map = exprStmt->expression->as<Wasp::MapLiteral>();
    ASSERT_EQ(map.pairs.size(), 0);
}

TEST(ParserTestSuite, Map) {
    auto mod = parse("{1 => 1, 2 => '2', 3 => 3.0}");
    ASSERT_EQ(mod.statements.size(), 1);

    auto* exprStmt = std::get_if<Wasp::ExpressionStatement>(&mod.statements[0]->data);
    ASSERT_NE(exprStmt, nullptr);

    ASSERT_TRUE(exprStmt->expression->is<Wasp::MapLiteral>());
    const auto& map = exprStmt->expression->as<Wasp::MapLiteral>();
    ASSERT_EQ(map.pairs.size(), 3);    
}
