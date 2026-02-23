#include "token.h"
#include "lexer.h"
#include "parser.h"
#include "test_utils.h"
#include <gtest/gtest.h>


TEST(ParseExpressions, ReturnStatementExpression) {
    auto mod = parse(R"(
return 5 + 23
)");
    ASSERT_EQ(mod.statements.size(), 1);

    auto& stmt = check<Wasp::Return>(mod.statements[0]);
    ASSERT_TRUE(stmt.expression.has_value()) << "Expected an expression in return statement";

    auto& return_expr = stmt.expression.value();

    auto& infix_expr = check<Wasp::Infix>(return_expr);
    EXPECT_EQ(infix_expr.op.type, Wasp::TokenType::PLUS) << "Expected '+' operator in return expression";
    EXPECT_EQ(check<int>(infix_expr.left), 5);
    EXPECT_EQ(check<int>(infix_expr.right), 23);
}

TEST(ParseExpressions, ReturnStatementEmpty) {
    auto mod = parse(R"(
return
    )");

    ASSERT_EQ(mod.statements.size(), 1);

    auto& stmt = check<Wasp::Return>(mod.statements[0]);
    EXPECT_FALSE(stmt.expression.has_value()) << "Expected no expression in return statement";
}

TEST(ParseOthers, AnnotationDefinitionSimple) {
    auto mod = parse("@tag('smoke', 'unit')");
    ASSERT_EQ(mod.statements.size(), 1);

    auto& stmt = check<Wasp::AnnotationDefinition>(mod.statements[0]);
    EXPECT_EQ(stmt.name, "tag");
    ASSERT_EQ(stmt.anno_values.size(), 2);
    
    auto& arg1 = check<std::string>(stmt.anno_values[0]);
    EXPECT_EQ(arg1, "smoke"); 

    auto& arg2 = check<std::string>(stmt.anno_values[1]);
    EXPECT_EQ(arg2, "unit");
}
