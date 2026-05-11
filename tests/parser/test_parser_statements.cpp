#include "Expression.h"
#include "Statement.h"
#include "Token.h"
#include "test_utils.h"

#include <gtest/gtest.h>
#include <string>

TEST(ParseExpressions, ReturnStatementExpression)
{
    auto block = parse(R"(
return 5 + 23
)");
    ASSERT_EQ(block.size(), 1);

    auto& stmt = check<Wasp::Return>(block[0]);
    ASSERT_TRUE(stmt.expression.has_value())
        << "Expected an expression in return statement";

    auto& return_expr = stmt.expression.value();

    auto& infix_expr = check<Wasp::Infix>(return_expr);
    EXPECT_EQ(infix_expr.op.type, Wasp::TokenType::PLUS)
        << "Expected '+' operator in return expression";

    auto& left_val = check<Wasp::IntegerLiteral>(infix_expr.left);
    EXPECT_EQ(left_val.value, 5);

    auto& right_val = check<Wasp::IntegerLiteral>(infix_expr.right);
    EXPECT_EQ(right_val.value, 23);
}

TEST(ParseExpressions, ReturnStatementEmpty)
{
    auto block = parse(R"(
return
    )");

    ASSERT_EQ(block.size(), 1);

    auto& stmt = check<Wasp::Return>(block[0]);
    EXPECT_FALSE(stmt.expression.has_value())
        << "Expected no expression in return statement";
}
