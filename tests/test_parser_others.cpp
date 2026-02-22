#include "token.h"
#include "lexer.h"
#include "parser.h"
#include "test_utils.h"
#include <gtest/gtest.h>


TEST(ParseOthers, AnnotationDefinitionSimple) {
    auto mod = parse("@tag('smoke', 'unit')");
    ASSERT_EQ(mod.statements.size(), 1);

    // Unpack the AnnotationDefinition directly from the statement
    auto* annotation = std::get_if<Wasp::AnnotationDefinition>(&mod.statements[0]->data);
    ASSERT_NE(annotation, nullptr) << "Expected an AnnotationDefinition";

    // Verify the annotation name
    EXPECT_EQ(annotation->name, "tag");

    // Verify the arguments passed to the annotation
    ASSERT_EQ(annotation->anno_values.size(), 2);

    // First arg: 'smoke'
    ASSERT_NE(annotation->anno_values[0], nullptr);
    auto* arg1 = std::get_if<std::string>(&annotation->anno_values[0]->data);
    ASSERT_NE(arg1, nullptr);
    EXPECT_EQ(*arg1, "smoke"); // Assuming the parser strips the quotes

    // Second arg: 'unit'
    ASSERT_NE(annotation->anno_values[1], nullptr);
    auto* arg2 = std::get_if<std::string>(&annotation->anno_values[1]->data);
    ASSERT_NE(arg2, nullptr);
    EXPECT_EQ(*arg2, "unit");
}

TEST(ParseExpressions, ReturnStatementExpression) {
    auto mod = parse(R"(return 5 + 23)");
    ASSERT_EQ(mod.statements.size(), 1);

    auto* return_stmt = std::get_if<Wasp::Return>(&mod.statements[0]->data);
    ASSERT_NE(return_stmt, nullptr) << "Expected a Return statement";
    
    // Ensure the optional has a value
    ASSERT_TRUE(return_stmt->expression.has_value());
    
    // Safely verify the expression inside the return
    auto expr = return_stmt->expression.value();
    ASSERT_NE(expr, nullptr) << "Return expression pointer is null";
    
    // "5 + 23" should parse as an Infix expression
    ASSERT_TRUE(expr->is<Wasp::Infix>()); 
}

TEST(ParseExpressions, ReturnStatementEmpty) {
    auto mod = parse(R"(return)");
    ASSERT_EQ(mod.statements.size(), 1);

    // Unpack the variant instead of casting the pointer
    auto* return_stmt = std::get_if<Wasp::Return>(&mod.statements[0]->data);
    ASSERT_NE(return_stmt, nullptr) << "Expected a Return statement";
    
    EXPECT_FALSE(return_stmt->expression.has_value());
}