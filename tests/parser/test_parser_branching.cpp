#include "Expression.h"
#include "Statement.h"
#include "Token.h"
#include "TypeAnnotation.h"

#include "test_utils.h"
#include <gtest/gtest.h>

TEST(ParseBranching, TernaryExpression)
{
    auto block = parse("if true then 1 else 2");

    auto& stmt = check<Wasp::ExpressionStatement>(block[0]);

    auto& ternary = check<Wasp::TernaryExpression>(stmt.expression);

    ASSERT_NE(ternary.test, nullptr);
    ASSERT_TRUE(ternary.test->is<Wasp::BooleanLiteral>());
    EXPECT_EQ(ternary.test->as<Wasp::BooleanLiteral>().value, true);

    ASSERT_NE(ternary.then_expr, nullptr);
    ASSERT_TRUE(ternary.then_expr->is<Wasp::IntegerLiteral>());
    EXPECT_EQ(ternary.then_expr->as<Wasp::IntegerLiteral>().value, 1);

    ASSERT_NE(ternary.else_expr, nullptr);
    ASSERT_TRUE(ternary.else_expr->is<Wasp::IntegerLiteral>());
    EXPECT_EQ(ternary.else_expr->as<Wasp::IntegerLiteral>().value, 2);
}

TEST(ParseBranching, TernaryLetExpression)
{
    auto block = parse("if let x = 1 then 1 else 2");

    auto& stmt = check<Wasp::ExpressionStatement>(block[0]);
    auto& ternary = check<Wasp::TernaryExpression>(stmt.expression);

    // Test Condition (Binding node for variable definition)
    auto& binding = check<Wasp::Binding>(ternary.test);
    EXPECT_TRUE(binding.is_mutable); // 'let' makes it mutable

    auto& identifier = check<Wasp::Identifier>(binding.lhs);
    EXPECT_EQ(identifier.name, "x");

    auto& binding_value = check<Wasp::IntegerLiteral>(binding.rhs);
    EXPECT_EQ(binding_value.value, 1);

    // TRUE Branch
    auto& true_val = check<Wasp::IntegerLiteral>(ternary.then_expr);
    EXPECT_EQ(true_val.value, 1);

    // ELSE Branch
    auto& false_val = check<Wasp::IntegerLiteral>(ternary.else_expr);
    EXPECT_EQ(false_val.value, 2);
}

TEST(ParseBranching, IfBlock)
{
    auto block = parse(R"(
if true then
    1
)");
    auto& stmt = check<Wasp::Branch>(block[0]);

    auto& body = stmt.block.statements;
    ASSERT_EQ(body.size(), 1);

    auto& test = check<Wasp::BooleanLiteral>(stmt.test);
    EXPECT_EQ(test.value, true);

    auto& innerStmt = check<Wasp::ExpressionStatement>(body[0]);
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
    auto& stmt = check<Wasp::Branch>(block[0]);
    auto& test = check<Wasp::Infix>(stmt.test);

    {
        auto& left = check<Wasp::Identifier>(test.left);
        EXPECT_EQ(left.name, "x");

        EXPECT_EQ(test.op.type, Wasp::TokenType::EQUAL_EQUAL);

        auto& right = check<Wasp::IntegerLiteral>(test.right);
        EXPECT_EQ(right.value, 25);
    }

    {
        ASSERT_EQ(stmt.block.statements.size(), 1);
        check<Wasp::Pass>(stmt.block.statements[0]);
    }

    // 2. Check the 'elif' branch (nested in the first 'if' else_expr)
    ASSERT_TRUE(stmt.alternative);
    auto& elif_stmt = check<Wasp::Branch>(stmt.alternative);

    {
        auto& elif_test = check<Wasp::Infix>(elif_stmt.test);

        auto& left = check<Wasp::Identifier>(elif_test.left);
        EXPECT_EQ(left.name, "x");

        EXPECT_EQ(elif_test.op.type, Wasp::TokenType::EQUAL_EQUAL);

        auto& right = check<Wasp::IntegerLiteral>(elif_test.right);
        EXPECT_EQ(right.value, 30);
    }

    {
        ASSERT_EQ(elif_stmt.block.statements.size(), 1);
        check<Wasp::Pass>(elif_stmt.block.statements[0]);
    }

    // 3. Check the 'else' branch (nested in the 'elif' else_expr)
    ASSERT_TRUE(elif_stmt.alternative);
    auto& else_stmt = check<Wasp::Branch>(elif_stmt.alternative);

    {
        ASSERT_EQ(else_stmt.block.statements.size(), 1);
        check<Wasp::Pass>(else_stmt.block.statements[0]);
    }
}
