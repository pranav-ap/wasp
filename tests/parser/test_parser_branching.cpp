#include "Expression.h"
#include "Statement.h"
#include "Token.h"
#include "test_utils.h"
#include <gtest/gtest.h>

TEST(ParseBranching, TernaryExpression)
{
    auto block = parse("if true then 1 else 2");

    auto& stmt = check<Wasp::ExpressionStatement>(block[0]);

    auto& ternary = check<Wasp::IfTernaryBranch>(stmt.expression);
    ASSERT_NE(ternary.test, nullptr);
    ASSERT_TRUE(ternary.test->is<Wasp::BooleanLiteral>());
    EXPECT_EQ(ternary.test->as<Wasp::BooleanLiteral>().value, true);

    ASSERT_NE(ternary.true_expression, nullptr);
    ASSERT_TRUE(ternary.true_expression->is<Wasp::IntegerLiteral>());
    EXPECT_EQ(ternary.true_expression->as<Wasp::IntegerLiteral>().value, 1);

    ASSERT_NE(ternary.alternative, nullptr);
    ASSERT_TRUE(ternary.alternative->is<Wasp::ElseTernaryBranch>());

    auto& elseTernary = ternary.alternative->as<Wasp::ElseTernaryBranch>();
    ASSERT_NE(elseTernary.expression, nullptr);
    ASSERT_TRUE(elseTernary.expression->is<Wasp::IntegerLiteral>());
    EXPECT_EQ(elseTernary.expression->as<Wasp::IntegerLiteral>().value, 2);
}

TEST(ParseBranching, TernaryLetExpression)
{
    auto block = parse("if let x = 1 then 1 else 2");

    auto& stmt = check<Wasp::ExpressionStatement>(block[0]);
    auto& ternary = check<Wasp::IfTernaryBranch>(stmt.expression);

    // Test Condition (now a unified Assignment node)
    auto& assign = check<Wasp::Assignment>(ternary.test);
    EXPECT_TRUE(assign.is_definition);
    EXPECT_TRUE(assign.is_mutable); // 'let' makes it mutable

    auto& identifier = check<Wasp::Identifier>(assign.lhs);
    EXPECT_EQ(identifier.name, "x");

    auto& assign_value = check<Wasp::IntegerLiteral>(assign.rhs);
    EXPECT_EQ(assign_value.value, 1);

    // TRUE Branch
    auto& true_val = check<Wasp::IntegerLiteral>(ternary.true_expression);
    EXPECT_EQ(true_val.value, 1);

    // ELSE Branch
    auto& else_branch = check<Wasp::ElseTernaryBranch>(ternary.alternative);
    auto& false_val = check<Wasp::IntegerLiteral>(else_branch.expression);
    EXPECT_EQ(false_val.value, 2);
}

TEST(ParseBranching, IfBlock)
{
    auto block = parse(R"(
if true then
    1
)");
    auto& stmt = check<Wasp::IfBranch>(block[0]);

    auto &body = stmt.body;
    ASSERT_EQ(body.size(), 1);

    auto& test = check<Wasp::BooleanLiteral>(stmt.test);
    EXPECT_EQ(test.value, true);

    auto &innerStmt = check<Wasp::ExpressionStatement>(body[0]);
    auto& innerValue = check<Wasp::IntegerLiteral>(innerStmt.expression);
    EXPECT_EQ(innerValue.value, 1);
}

TEST(ParseBranching, IfElifElseBlock)
{
    auto block = parse(R"(
if x == 25 then
    pass
elif x == 30 then
    pass
else
    pass
)");

    // 1. Check the main 'if' branch
    auto& stmt = check<Wasp::IfBranch>(block[0]);
    auto& test = check<Wasp::Infix>(stmt.test);

    {
        auto& left = check<Wasp::Identifier>(test.left);
        EXPECT_EQ(left.name, "x");

        EXPECT_EQ(test.op.type, Wasp::TokenType::EQUAL_EQUAL);

        auto& right = check<Wasp::IntegerLiteral>(test.right);
        EXPECT_EQ(right.value, 25);
    }

    {
        ASSERT_EQ(stmt.body.size(), 1);
        check<Wasp::Placeholder>(stmt.body[0]);
    }

    // 2. Check the 'elif' branch (nested in the first 'if' alternative)
    ASSERT_TRUE(stmt.alternative);
    auto& elif_stmt = check<Wasp::IfBranch>(stmt.alternative);

    {
        auto& elif_test = check<Wasp::Infix>(elif_stmt.test);

        auto& left = check<Wasp::Identifier>(elif_test.left);
        EXPECT_EQ(left.name, "x");

        EXPECT_EQ(elif_test.op.type, Wasp::TokenType::EQUAL_EQUAL);

        auto& right = check<Wasp::IntegerLiteral>(elif_test.right);
        EXPECT_EQ(right.value, 30);
    }

    {
        ASSERT_EQ(elif_stmt.body.size(), 1);
        check<Wasp::Placeholder>(elif_stmt.body[0]);
    }

    // 3. Check the 'else' branch (nested in the 'elif' alternative)
    ASSERT_TRUE(elif_stmt.alternative);
    auto& else_stmt = check<Wasp::ElseBranch>(elif_stmt.alternative);

    {
        ASSERT_EQ(else_stmt.body.size(), 1);
        check<Wasp::Placeholder>(else_stmt.body[0]);
    }
}
