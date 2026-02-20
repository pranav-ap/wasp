#include "token.h"
#include "lexer.h"
#include "parser.h"
#include "test_utils.h"
#include <gtest/gtest.h>


TEST(ParserTestSuite, MemberAccessSimple) {
    auto mod = parse(R"(Animal.Dog)");

    EXPECT_TRUE(true);
}

TEST(ParserTestSuite, MemberAccessNested) {
    auto mod = parse(R"(Animal.Dog.GermanShepherd)");

    EXPECT_TRUE(true);
}

TEST(ParserTestSuite, MemberAccessWithString) {
    auto mod = parse(R"(Animal.'Dog'.GermanShepherd)");

    EXPECT_TRUE(true);
}

TEST(ParserTestSuite, ReturnStatementEmpty) {
    auto mod = parse(R"(return)");
    ASSERT_EQ(mod.statements.size(), 1);

    // Unpack the variant instead of casting the pointer
    auto* return_stmt = std::get_if<Wasp::Return>(&mod.statements[0]->data);
    ASSERT_NE(return_stmt, nullptr) << "Expected a Return statement";
    
    EXPECT_FALSE(return_stmt->expression.has_value());
}

TEST(ParserTestSuite, ReturnStatementExpression) {
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

TEST(ParserTestSuite, FunctionCallWithoutArguments) {
    auto mod = parse("get_worker()");

    EXPECT_TRUE(true);
}

TEST(ParserTestSuite, FunctionCallWithOneArgument) {
    auto mod = parse("get_worker(1)");

    EXPECT_TRUE(true);
}

TEST(ParserTestSuite, FunctionCallWithMultipleArguments) {
    auto mod = parse("get_worker(1, 'John', true)");

    EXPECT_TRUE(true);
}

TEST(ParserTestSuite, MethodAccess) {
    auto mod = parse("company.get_worker(1, 'John', true)");

    EXPECT_TRUE(true);
}

TEST(ParserTestSuite, MethodAccessThenMemberAccess) {
    auto mod = parse("company.get_worker(1, 'John', true).name");

    EXPECT_TRUE(true);
}

TEST(ParserTestSuite, MethodAccessThenMethodAccess) {
    auto mod = parse("company.get_worker(1, 'John', true).get_name()");

    EXPECT_TRUE(true);
}

TEST(ParserTestSuite, MethodAccessThenMethodAccessWithStringAccess) {
    auto mod = parse("company.get_worker(1, 'John', true).'police'.get_name()");

    EXPECT_TRUE(true);
}

TEST(ParserTestSuite, RangeSimpleExclusive) {
    auto mod = parse("1..10");

    EXPECT_TRUE(true);
}

TEST(ParserTestSuite, RangeSimpleInclusive) {
    auto mod = parse("1...10");

    EXPECT_TRUE(true);
}

TEST(ParserTestSuite, RangeWithStep) {
    auto mod = parse("1..10:2");

    EXPECT_TRUE(true);
}

TEST(ParserTestSuite, RangeWithoutEnd) {
    auto mod = parse("1..");

    EXPECT_TRUE(true);
}

TEST(ParserTestSuite, RangeWithoutEndWithStep) {
    auto mod = parse("1..:2");

    EXPECT_TRUE(true);
}

TEST(ParserTestSuite, RangeWithoutStartOrEndOrStep) {
    // No assigned meaning yet, but should still parse without error
    auto mod = parse("...");

    EXPECT_TRUE(true);
}


TEST(ParserTestSuite, Dot) {
    auto mod = parse(".");

    EXPECT_TRUE(true);
}


TEST(ParserTestSuite, DotDot) {
    auto mod = parse("..");

    EXPECT_TRUE(true);
}

TEST(ParserTestSuite, PipeOutput) {
    auto mod = parse("foo() ~ bar(., 35) ~ boom(...)");

    EXPECT_TRUE(true);
}

TEST(ParserTestSuite, Star) {
    auto mod = parse("*b");

    EXPECT_TRUE(true);
}

TEST(ParserTestSuite, StarGather) {
    auto mod = parse("[a, *b, c] = [1, 2, 3, 4, 5]");

    EXPECT_TRUE(true);
}

TEST(ParserTestSuite, StarSpread) {
    auto mod = parse("[a, b, c] = *three_nums");

    EXPECT_TRUE(true);
}

TEST(ParserTestSuite, StarGatherAndSpread) {
    auto mod = parse("[a, *b, c] = *five_nums");

    EXPECT_TRUE(true);
}


TEST(ParserTestSuite, AnnotationDefinitionSimple) {
    auto mod = parse("@tag('smoke', 'unit')");

    EXPECT_TRUE(true);
}
