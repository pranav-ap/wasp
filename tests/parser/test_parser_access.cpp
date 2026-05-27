#include "Expression.h"
#include "Statement.h"
#include "test_utils.h"
#include <gtest/gtest.h>
#include <string>

TEST(ParseExpressions, MemberAccessNested) {
    auto block = parse("Animal.Dog.GermanShepherd");
    ASSERT_EQ(block.size(), 1);

    auto& expr_stmt = check<Wasp::ExpressionStatement>(block[0]);

    // Top Level: [Animal.Dog] . [GermanShepherd]
    auto& outer_access = check<Wasp::MemberAccess>(expr_stmt.expression);

    auto& right_id = check<Wasp::Identifier>(outer_access.right);
    EXPECT_EQ(right_id.name, "GermanShepherd");

    // Inner Level: [Animal] . [Dog]
    auto& inner_access = check<Wasp::MemberAccess>(outer_access.left);

    auto& inner_right_id = check<Wasp::Identifier>(inner_access.right);
    EXPECT_EQ(inner_right_id.name, "Dog");

    auto& inner_left_id = check<Wasp::Identifier>(inner_access.left);
    EXPECT_EQ(inner_left_id.name, "Animal");
}

TEST(ParseExpressions, FunctionCallWithoutArguments) {
    auto block = parse("get_worker()");
    ASSERT_EQ(block.size(), 1);

    auto& stmt = check<Wasp::ExpressionStatement>(block[0]);
    auto& call = check<Wasp::Call>(stmt.expression);

    auto& id = check<Wasp::Identifier>(call.callable);
    EXPECT_EQ(id.name, "get_worker");
    EXPECT_EQ(call.arguments.size(), 0);
}

TEST(ParseExpressions, FunctionCallWithMultipleArguments) {
    auto block = parse("get_worker(1, 'John', true)");
    ASSERT_EQ(block.size(), 1);

    auto& stmt = check<Wasp::ExpressionStatement>(block[0]);
    ASSERT_NE(stmt.expression, nullptr);

    auto& call = check<Wasp::Call>(stmt.expression);
    ASSERT_EQ(call.arguments.size(), 3);
}

TEST(ParseExpressions, MethodCallWithArguments) {
    auto block = parse("company.get_worker(1, 'John', true)");
    ASSERT_EQ(block.size(), 1);

    auto& stmt = check<Wasp::ExpressionStatement>(block[0]);

    // Top Level is now a pure Call node
    auto& call = check<Wasp::Call>(stmt.expression);
    EXPECT_EQ(call.arguments.size(), 3);

    // The callable being executed is a MemberAccess node
    auto& callee_access = check<Wasp::MemberAccess>(call.callable);

    auto& left_id = check<Wasp::Identifier>(callee_access.left);
    EXPECT_EQ(left_id.name, "company");

    auto& right_id = check<Wasp::Identifier>(callee_access.right);
    EXPECT_EQ(right_id.name, "get_worker");
}

TEST(ParseExpressions, MethodCallWithArgumentsThenMemberAccess) {
    auto block = parse("company.get_worker(1, 'John', true).name");
    ASSERT_EQ(block.size(), 1);

    auto& stmt = check<Wasp::ExpressionStatement>(block[0]);

    // Top Level: [company.get_worker(...)] . [name]
    auto& outer_access = check<Wasp::MemberAccess>(stmt.expression);

    auto& property_id = check<Wasp::Identifier>(outer_access.right);
    EXPECT_EQ(property_id.name, "name");

    // The left side is the Call node!
    auto& call = check<Wasp::Call>(outer_access.left);
    EXPECT_EQ(call.arguments.size(), 3);

    auto& inner_access = check<Wasp::MemberAccess>(call.callable);

    auto& left_id = check<Wasp::Identifier>(inner_access.left);
    EXPECT_EQ(left_id.name, "company");

    auto& right_id = check<Wasp::Identifier>(inner_access.right);
    EXPECT_EQ(right_id.name, "get_worker");
}

TEST(ParseExpressions, FunctionCallThenMemberAccess) {
    auto block = parse("get_company().worker");
    ASSERT_EQ(block.size(), 1);

    auto& stmt = check<Wasp::ExpressionStatement>(block[0]);

    // Top Level: [get_company()] . [worker]
    auto& access = check<Wasp::MemberAccess>(stmt.expression);

    auto& property_id = check<Wasp::Identifier>(access.right);
    EXPECT_EQ(property_id.name, "worker");

    // The left side is a Call node
    auto& call = check<Wasp::Call>(access.left);
    EXPECT_EQ(call.arguments.size(), 0);

    auto& callee_id = check<Wasp::Identifier>(call.callable);
    EXPECT_EQ(callee_id.name, "get_company");
}

TEST(ParseExpressions, FunctionCallThenMethodCall) {
    auto block = parse("get_company().get_worker(1)");
    ASSERT_EQ(block.size(), 1);

    auto& stmt = check<Wasp::ExpressionStatement>(block[0]);

    // Top Level: A Call because the chain ends in an execution: (...)
    auto& outer_call = check<Wasp::Call>(stmt.expression);
    EXPECT_EQ(outer_call.arguments.size(), 1);

    // The callable is the MemberAccess: [get_company()] . [get_worker]
    auto& access = check<Wasp::MemberAccess>(outer_call.callable);

    auto& method_id = check<Wasp::Identifier>(access.right);
    EXPECT_EQ(method_id.name, "get_worker");

    // The left side of the member access is the initial Call
    auto& left_call = check<Wasp::Call>(access.left);
    EXPECT_EQ(left_call.arguments.size(), 0);

    auto& left_callee = check<Wasp::Identifier>(left_call.callable);
    EXPECT_EQ(left_callee.name, "get_company");
}
