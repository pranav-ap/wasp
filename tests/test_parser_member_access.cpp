#include "token.h"
#include "lexer.h"
#include "parser.h"
#include "test_utils.h"
#include <gtest/gtest.h>


TEST(ParseExpressions, MemberAccessSimple) {
    auto mod = parse(R"(Animal.Dog)");
    ASSERT_EQ(mod.statements.size(), 1);

    auto& stmt = check<Wasp::ExpressionStatement>(mod.statements[0]);
    ASSERT_TRUE(stmt.expression->is<Wasp::Infix>());
    const auto& infix = stmt.expression->as<Wasp::Infix>();

    ASSERT_TRUE(infix.left->is<Wasp::Identifier>());
    const auto& leftId = infix.left->as<Wasp::Identifier>();
    EXPECT_EQ(leftId.name, "Animal");
    ASSERT_TRUE(infix.right->is<Wasp::Identifier>());
    const auto& rightId = infix.right->as<Wasp::Identifier>();
    EXPECT_EQ(rightId.name, "Dog");
}

TEST(ParseExpressions, MemberAccessNested) {
    auto mod = parse(R"(Animal.Dog.GermanShepherd)");

    EXPECT_TRUE(true);
}

TEST(ParseExpressions, MemberAccessWithString) {
    auto mod = parse(R"(Animal.'Dog'.GermanShepherd)");

    EXPECT_TRUE(true);
}


TEST(ParseExpressions, FunctionCallWithoutArguments) {
    auto mod = parse("get_worker()");
    ASSERT_EQ(mod.statements.size(), 1);

    auto* stmt = std::get_if<Wasp::ExpressionStatement>(&mod.statements[0]->data);
    ASSERT_NE(stmt, nullptr);
    ASSERT_NE(stmt->expression, nullptr);

    auto* call = std::get_if<Wasp::Call>(&stmt->expression->data);
    ASSERT_NE(call, nullptr) << "Expected a Call expression";

    ASSERT_NE(call->callee, nullptr);
    auto* calleeId = std::get_if<Wasp::Identifier>(&call->callee->data);
    ASSERT_NE(calleeId, nullptr);
    EXPECT_EQ(calleeId->name, "get_worker");

    EXPECT_EQ(call->arguments.size(), 0);
}

TEST(ParseExpressions, FunctionCallWithOneArgument) {
    auto mod = parse("get_worker(1)");
    ASSERT_EQ(mod.statements.size(), 1);

    auto* stmt = std::get_if<Wasp::ExpressionStatement>(&mod.statements[0]->data);
    ASSERT_NE(stmt, nullptr);
    ASSERT_NE(stmt->expression, nullptr);

    auto* call = std::get_if<Wasp::Call>(&stmt->expression->data);
    ASSERT_NE(call, nullptr);

    ASSERT_EQ(call->arguments.size(), 1);
    
    ASSERT_NE(call->arguments[0], nullptr);
    auto* arg1 = std::get_if<int>(&call->arguments[0]->data);
    ASSERT_NE(arg1, nullptr);
    EXPECT_EQ(*arg1, 1);
}

TEST(ParseExpressions, FunctionCallWithMultipleArguments) {
    auto mod = parse("get_worker(1, 'John', true)");
    ASSERT_EQ(mod.statements.size(), 1);

    auto* stmt = std::get_if<Wasp::ExpressionStatement>(&mod.statements[0]->data);
    ASSERT_NE(stmt, nullptr);
    ASSERT_NE(stmt->expression, nullptr);

    auto* call = std::get_if<Wasp::Call>(&stmt->expression->data);
    ASSERT_NE(call, nullptr);
    ASSERT_EQ(call->arguments.size(), 3);

    ASSERT_NE(call->arguments[0], nullptr);
    auto* arg1 = std::get_if<int>(&call->arguments[0]->data);
    ASSERT_NE(arg1, nullptr);
    EXPECT_EQ(*arg1, 1);

    ASSERT_NE(call->arguments[1], nullptr);
    auto* arg2 = std::get_if<std::string>(&call->arguments[1]->data);
    ASSERT_NE(arg2, nullptr);
    EXPECT_EQ(*arg2, "John"); 
    
    ASSERT_NE(call->arguments[2], nullptr);
    auto* arg3 = std::get_if<bool>(&call->arguments[2]->data);
    ASSERT_NE(arg3, nullptr);
    EXPECT_EQ(*arg3, true);
}

TEST(ParseExpressions, MethodAccess) {
    auto mod = parse("company.get_worker(1, 'John', true)");
    ASSERT_EQ(mod.statements.size(), 1);

    auto* stmt = std::get_if<Wasp::ExpressionStatement>(&mod.statements[0]->data);
    ASSERT_NE(stmt, nullptr);
    ASSERT_NE(stmt->expression, nullptr);

    auto* call = std::get_if<Wasp::Call>(&stmt->expression->data);
    ASSERT_NE(call, nullptr);
    ASSERT_EQ(call->arguments.size(), 3); 

    ASSERT_NE(call->callee, nullptr);
    auto* infix = std::get_if<Wasp::Infix>(&call->callee->data);
    ASSERT_NE(infix, nullptr) << "Expected callee to be an Infix expression (dot access)";

    ASSERT_NE(infix->left, nullptr);
    auto* leftId = std::get_if<Wasp::Identifier>(&infix->left->data);
    ASSERT_NE(leftId, nullptr);
    EXPECT_EQ(leftId->name, "company");

    ASSERT_NE(infix->right, nullptr);
    auto* rightId = std::get_if<Wasp::Identifier>(&infix->right->data);
    ASSERT_NE(rightId, nullptr);
    EXPECT_EQ(rightId->name, "get_worker");
}

TEST(ParseExpressions, MethodAccessThenMemberAccess) {
    auto mod = parse("company.get_worker(1, 'John', true).name");
    ASSERT_EQ(mod.statements.size(), 1);

    auto* stmt = std::get_if<Wasp::ExpressionStatement>(&mod.statements[0]->data);
    ASSERT_NE(stmt, nullptr);
    ASSERT_NE(stmt->expression, nullptr);

    auto* rootInfix = std::get_if<Wasp::Infix>(&stmt->expression->data);
    ASSERT_NE(rootInfix, nullptr);

    ASSERT_NE(rootInfix->left, nullptr);
    auto* leftCall = std::get_if<Wasp::Call>(&rootInfix->left->data);
    ASSERT_NE(leftCall, nullptr);

    ASSERT_NE(rootInfix->right, nullptr);
    auto* rightId = std::get_if<Wasp::Identifier>(&rootInfix->right->data);
    ASSERT_NE(rightId, nullptr);
    EXPECT_EQ(rightId->name, "name");
}

TEST(ParseExpressions, MethodAccessThenMethodAccess) {
    auto mod = parse("company.get_worker(1, 'John', true).get_name()");
    ASSERT_EQ(mod.statements.size(), 1);

    auto* stmt = std::get_if<Wasp::ExpressionStatement>(&mod.statements[0]->data);
    ASSERT_NE(stmt, nullptr);
    ASSERT_NE(stmt->expression, nullptr);

    auto* rootCall = std::get_if<Wasp::Call>(&stmt->expression->data);
    ASSERT_NE(rootCall, nullptr);
    EXPECT_EQ(rootCall->arguments.size(), 0);

    ASSERT_NE(rootCall->callee, nullptr);
    auto* calleeInfix = std::get_if<Wasp::Infix>(&rootCall->callee->data);
    ASSERT_NE(calleeInfix, nullptr);

    ASSERT_NE(calleeInfix->left, nullptr);
    auto* firstCall = std::get_if<Wasp::Call>(&calleeInfix->left->data);
    ASSERT_NE(firstCall, nullptr);

    ASSERT_NE(calleeInfix->right, nullptr);
    auto* rightId = std::get_if<Wasp::Identifier>(&calleeInfix->right->data);
    ASSERT_NE(rightId, nullptr);
    EXPECT_EQ(rightId->name, "get_name");
}

TEST(ParseExpressions, MethodAccessThenMethodAccessWithStringAccess) {
    auto mod = parse("company.get_worker(1, 'John', true).'police'.get_name()");
    ASSERT_EQ(mod.statements.size(), 1);

    auto* stmt = std::get_if<Wasp::ExpressionStatement>(&mod.statements[0]->data);
    ASSERT_NE(stmt, nullptr);
    ASSERT_NE(stmt->expression, nullptr);

    auto* rootCall = std::get_if<Wasp::Call>(&stmt->expression->data);
    ASSERT_NE(rootCall, nullptr);

    ASSERT_NE(rootCall->callee, nullptr);
    auto* topInfix = std::get_if<Wasp::Infix>(&rootCall->callee->data);
    ASSERT_NE(topInfix, nullptr);

    ASSERT_NE(topInfix->right, nullptr);
    auto* get_name_id = std::get_if<Wasp::Identifier>(&topInfix->right->data);
    ASSERT_NE(get_name_id, nullptr);
    EXPECT_EQ(get_name_id->name, "get_name");

    ASSERT_NE(topInfix->left, nullptr);
    auto* midInfix = std::get_if<Wasp::Infix>(&topInfix->left->data);
    ASSERT_NE(midInfix, nullptr);

    ASSERT_NE(midInfix->right, nullptr);
    auto* policeStr = std::get_if<std::string>(&midInfix->right->data);
    ASSERT_NE(policeStr, nullptr);
    EXPECT_EQ(*policeStr, "police");

    ASSERT_NE(midInfix->left, nullptr);
    auto* firstCall = std::get_if<Wasp::Call>(&midInfix->left->data);
    ASSERT_NE(firstCall, nullptr);
}

TEST(ParseExpressions, PipeOutput) {
    auto mod = parse("foo() ~ bar(., 35) ~ boom(...)");
    ASSERT_EQ(mod.statements.size(), 1);

    auto* stmt = std::get_if<Wasp::ExpressionStatement>(&mod.statements[0]->data);
    ASSERT_NE(stmt, nullptr);
    ASSERT_NE(stmt->expression, nullptr);

    auto* rootInfix = std::get_if<Wasp::Infix>(&stmt->expression->data);
    ASSERT_NE(rootInfix, nullptr);

    ASSERT_NE(rootInfix->right, nullptr);
    auto* rightCall = std::get_if<Wasp::Call>(&rootInfix->right->data);
    ASSERT_NE(rightCall, nullptr);
    ASSERT_EQ(rightCall->arguments.size(), 1);
    auto* boomArg = std::get_if<Wasp::DotDotDotLiteral>(&rightCall->arguments[0]->data);
    ASSERT_NE(boomArg, nullptr);

    ASSERT_NE(rootInfix->left, nullptr);
    auto* leftInfix = std::get_if<Wasp::Infix>(&rootInfix->left->data);
    ASSERT_NE(leftInfix, nullptr);

    ASSERT_NE(leftInfix->right, nullptr);
    auto* barCall = std::get_if<Wasp::Call>(&leftInfix->right->data);
    ASSERT_NE(barCall, nullptr);
    ASSERT_EQ(barCall->arguments.size(), 2);
    auto* barArg1 = std::get_if<Wasp::DotLiteral>(&barCall->arguments[0]->data);
    ASSERT_NE(barArg1, nullptr);
    auto* barArg2 = std::get_if<int>(&barCall->arguments[1]->data);
    ASSERT_NE(barArg2, nullptr);
    EXPECT_EQ(*barArg2, 35);

    ASSERT_NE(leftInfix->left, nullptr);
    auto* fooCall = std::get_if<Wasp::Call>(&leftInfix->left->data);
    ASSERT_NE(fooCall, nullptr);
    EXPECT_EQ(fooCall->arguments.size(), 0);
}
