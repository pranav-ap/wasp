#include "Token.h"
#include "Lexer.h"
#include "Parser.h"
#include "test_utils.h"
#include <gtest/gtest.h>

TEST(ParseBranching, TernaryExpression)
{
    auto mod = parse("if true then 1 else 2");

    auto &stmt = check<Wasp::ExpressionStatement>(mod.statements[0]);

    auto &ternary = check<Wasp::IfTernaryBranch>(stmt.expression);
    ASSERT_NE(ternary.test, nullptr) << "Ternary test condition is null";
    ASSERT_TRUE(ternary.test->is<bool>());
    EXPECT_EQ(ternary.test->as<bool>(), true);

    ASSERT_NE(ternary.true_expression, nullptr) << "Ternary true_expression is null";
    ASSERT_TRUE(ternary.true_expression->is<int>());
    EXPECT_EQ(ternary.true_expression->as<int>(), 1);

    ASSERT_NE(ternary.alternative, nullptr) << "Ternary alternative is null";
    ASSERT_TRUE(ternary.alternative->is<Wasp::ElseTernaryBranch>());

    auto &elseTernary = ternary.alternative->as<Wasp::ElseTernaryBranch>();
    ASSERT_NE(elseTernary.expression, nullptr) << "Else branch inner expression is null";
    ASSERT_TRUE(elseTernary.expression->is<int>());
    EXPECT_EQ(elseTernary.expression->as<int>(), 2);
}

TEST(ParseBranching, TernaryLetExpression)
{
    auto mod = parse("if let x = 1 then 1 else 2");

    auto &stmt = check<Wasp::ExpressionStatement>(mod.statements[0]);
    auto &ternary = check<Wasp::IfTernaryBranch>(stmt.expression);

    // Test Condition
    auto &letExpr = check<Wasp::VariableDefinitionExpression>(ternary.test);
    auto &assign = check<Wasp::UntypedAssignment>(letExpr.assignment);
    auto &identifier = check<Wasp::Identifier>(assign.lhs_expression);
    EXPECT_EQ(identifier.name, "x");
    auto &assign_value = check<int>(assign.rhs_expression);
    EXPECT_EQ(assign_value, 1);

    // TRUE Branch
    auto &true_val = check<int>(ternary.true_expression);
    EXPECT_EQ(true_val, 1);

    // ELSE Branch
    auto &else_branch = check<Wasp::ElseTernaryBranch>(ternary.alternative);
    auto &false_val = check<int>(else_branch.expression);
    EXPECT_EQ(false_val, 2);
}

TEST(ParseBranching, IfBlock)
{
    auto mod = parse(R"(
if true then
    1 
)");
    auto &stmt = check<Wasp::IfBranch>(mod.statements[0]);

    auto &body = stmt.body;
    ASSERT_EQ(body.size(), 1);

    auto &test = check<bool>(stmt.test);
    EXPECT_EQ(test, true);

    auto &innerStmt = check<Wasp::ExpressionStatement>(body[0]);
    auto &innerValue = check<int>(innerStmt.expression);
    EXPECT_EQ(innerValue, 1);
}

TEST(ParseBranching, IfElifElseBlock)
{
    auto mod = parse(R"(
if x == 25 then
    pass
elif x == 30 then
    pass
else
    pass
)");

    // 1. Check the main 'if' branch
    auto &stmt = check<Wasp::IfBranch>(mod.statements[0]);
    auto &test = check<Wasp::Infix>(stmt.test);

    {
        auto &left = check<Wasp::Identifier>(test.left);
        EXPECT_EQ(left.name, "x");

        EXPECT_EQ(test.op.type, Wasp::TokenType::EQUAL_EQUAL);

        auto &right = check<int>(test.right);
        EXPECT_EQ(right, 25);
    }

    {
        ASSERT_EQ(stmt.body.size(), 1);
        check<Wasp::Pass>(stmt.body[0]);
    }

    // 2. Check the 'elif' branch (nested in the first 'if' alternative)
    ASSERT_TRUE(stmt.alternative.has_value());
    auto &elif_stmt = check<Wasp::IfBranch>(stmt.alternative.value());

    {
        auto &elif_test = check<Wasp::Infix>(elif_stmt.test);

        auto &left = check<Wasp::Identifier>(elif_test.left);
        EXPECT_EQ(left.name, "x");

        EXPECT_EQ(elif_test.op.type, Wasp::TokenType::EQUAL_EQUAL);

        auto &right = check<int>(elif_test.right);
        EXPECT_EQ(right, 30);
    }

    {
        ASSERT_EQ(elif_stmt.body.size(), 1);
        check<Wasp::Pass>(elif_stmt.body[0]);
    }

    // 3. Check the 'else' branch (nested in the 'elif' alternative)
    ASSERT_TRUE(elif_stmt.alternative.has_value());
    auto &else_stmt = check<Wasp::ElseBranch>(elif_stmt.alternative.value());

    {
        ASSERT_EQ(else_stmt.body.size(), 1);
        check<Wasp::Pass>(else_stmt.body[0]);
    }
}
