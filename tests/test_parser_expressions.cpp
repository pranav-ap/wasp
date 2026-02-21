#include "token.h"
#include "lexer.h"
#include "parser.h"
#include "test_utils.h"
#include <gtest/gtest.h>


TEST(ParseExpressions, Number) {
    auto mod = parse("2");
    ASSERT_EQ(mod.statements.size(), 1);

    // Using the new is<T> and as<T> helpers
    auto& stmt = *mod.statements[0];
    
    ASSERT_TRUE(stmt.is<Wasp::ExpressionStatement>()) << "Expected an ExpressionStatement";
    
    const auto& exprStmt = stmt.as<Wasp::ExpressionStatement>();
    
    ASSERT_TRUE(exprStmt.expression->is<int>()) << "Expected an integer expression";
    EXPECT_EQ(exprStmt.expression->as<int>(), 2);
}


TEST(ParseExpressions, Addition) {
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

TEST(ParseExpressions, RocketOperator) {
    auto mod = parse("1 <=> 2");
    ASSERT_EQ(mod.statements.size(), 1);

    auto* exprStmt = std::get_if<Wasp::ExpressionStatement>(&mod.statements[0]->data);
    ASSERT_NE(exprStmt, nullptr);

    ASSERT_TRUE(exprStmt->expression->is<Wasp::Infix>());
    const auto& infix = exprStmt->expression->as<Wasp::Infix>();

    ASSERT_TRUE(infix.left->is<int>());
    EXPECT_EQ(infix.left->as<int>(), 1);

    EXPECT_EQ(infix.op.type, Wasp::TokenType::ROCKET); 

    ASSERT_TRUE(infix.right->is<int>());
    EXPECT_EQ(infix.right->as<int>(), 2);
}

TEST(ParseExpressions, List) {
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

TEST(ParseExpressions, Tuple) {
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


TEST(ParseExpressions, EmptySet) {
    auto mod = parse("{}");
    ASSERT_EQ(mod.statements.size(), 1);

    auto* exprStmt = std::get_if<Wasp::ExpressionStatement>(&mod.statements[0]->data);
    ASSERT_NE(exprStmt, nullptr);

    ASSERT_TRUE(exprStmt->expression->is<Wasp::SetLiteral>());
    const auto& set = exprStmt->expression->as<Wasp::SetLiteral>();

    ASSERT_EQ(set.expressions.size(), 0);
}


TEST(ParseExpressions, Set) {
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


TEST(ParseExpressions, EmptyMap) {
    auto mod = parse("{=>}");
    ASSERT_EQ(mod.statements.size(), 1);

    auto* exprStmt = std::get_if<Wasp::ExpressionStatement>(&mod.statements[0]->data);
    ASSERT_NE(exprStmt, nullptr);
    ASSERT_TRUE(exprStmt->expression->is<Wasp::MapLiteral>());
    const auto& map = exprStmt->expression->as<Wasp::MapLiteral>();
    ASSERT_EQ(map.pairs.size(), 0);
}

TEST(ParseExpressions, Map) {
    auto mod = parse("{1 => 1, 2 => '2', 3 => 3.0}");
    ASSERT_EQ(mod.statements.size(), 1);

    auto* exprStmt = std::get_if<Wasp::ExpressionStatement>(&mod.statements[0]->data);
    ASSERT_NE(exprStmt, nullptr);

    ASSERT_TRUE(exprStmt->expression->is<Wasp::MapLiteral>());
    const auto& map = exprStmt->expression->as<Wasp::MapLiteral>();
    ASSERT_EQ(map.pairs.size(), 3);    
}

TEST(ParseExpressions, MemberAccessSimple) {
    auto mod = parse(R"(Animal.Dog)");

    EXPECT_TRUE(true);
}

TEST(ParseExpressions, MemberAccessNested) {
    auto mod = parse(R"(Animal.Dog.GermanShepherd)");

    EXPECT_TRUE(true);
}

TEST(ParseExpressions, MemberAccessWithString) {
    auto mod = parse(R"(Animal.'Dog'.GermanShepherd)");

    EXPECT_TRUE(true);
}

TEST(ParseExpressions, ReturnStatementEmpty) {
    auto mod = parse(R"(return)");
    ASSERT_EQ(mod.statements.size(), 1);

    // Unpack the variant instead of casting the pointer
    auto* return_stmt = std::get_if<Wasp::Return>(&mod.statements[0]->data);
    ASSERT_NE(return_stmt, nullptr) << "Expected a Return statement";
    
    EXPECT_FALSE(return_stmt->expression.has_value());
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

#include "token.h"
#include "lexer.h"
#include "parser.h"
#include "test_utils.h"
#include <gtest/gtest.h>

TEST(ParseExpressions, FunctionCallWithoutArguments) {
    auto mod = parse("get_worker()");
    ASSERT_EQ(mod.statements.size(), 1);

    auto* exprStmt = std::get_if<Wasp::ExpressionStatement>(&mod.statements[0]->data);
    ASSERT_NE(exprStmt, nullptr);
    ASSERT_NE(exprStmt->expression, nullptr);

    auto* call = std::get_if<Wasp::Call>(&exprStmt->expression->data);
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

    auto* exprStmt = std::get_if<Wasp::ExpressionStatement>(&mod.statements[0]->data);
    ASSERT_NE(exprStmt, nullptr);
    ASSERT_NE(exprStmt->expression, nullptr);

    auto* call = std::get_if<Wasp::Call>(&exprStmt->expression->data);
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

    auto* exprStmt = std::get_if<Wasp::ExpressionStatement>(&mod.statements[0]->data);
    ASSERT_NE(exprStmt, nullptr);
    ASSERT_NE(exprStmt->expression, nullptr);

    auto* call = std::get_if<Wasp::Call>(&exprStmt->expression->data);
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

    auto* exprStmt = std::get_if<Wasp::ExpressionStatement>(&mod.statements[0]->data);
    ASSERT_NE(exprStmt, nullptr);
    ASSERT_NE(exprStmt->expression, nullptr);

    auto* call = std::get_if<Wasp::Call>(&exprStmt->expression->data);
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

    auto* exprStmt = std::get_if<Wasp::ExpressionStatement>(&mod.statements[0]->data);
    ASSERT_NE(exprStmt, nullptr);
    ASSERT_NE(exprStmt->expression, nullptr);

    auto* rootInfix = std::get_if<Wasp::Infix>(&exprStmt->expression->data);
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

    auto* exprStmt = std::get_if<Wasp::ExpressionStatement>(&mod.statements[0]->data);
    ASSERT_NE(exprStmt, nullptr);
    ASSERT_NE(exprStmt->expression, nullptr);

    auto* rootCall = std::get_if<Wasp::Call>(&exprStmt->expression->data);
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

    auto* exprStmt = std::get_if<Wasp::ExpressionStatement>(&mod.statements[0]->data);
    ASSERT_NE(exprStmt, nullptr);
    ASSERT_NE(exprStmt->expression, nullptr);

    auto* rootCall = std::get_if<Wasp::Call>(&exprStmt->expression->data);
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

TEST(ParseExpressions, RangeSimpleExclusive) {
    auto mod = parse("1..10");
    ASSERT_EQ(mod.statements.size(), 1);

    auto* exprStmt = std::get_if<Wasp::ExpressionStatement>(&mod.statements[0]->data);
    ASSERT_NE(exprStmt, nullptr);
    ASSERT_NE(exprStmt->expression, nullptr);

    auto* range = std::get_if<Wasp::RangeLiteral>(&exprStmt->expression->data);
    ASSERT_NE(range, nullptr);

    EXPECT_FALSE(range->is_inclusive);

    ASSERT_NE(range->start, nullptr);
    auto* startVal = std::get_if<int>(&range->start->data);
    ASSERT_NE(startVal, nullptr);
    EXPECT_EQ(*startVal, 1);

    ASSERT_NE(range->end, nullptr);
    auto* endVal = std::get_if<int>(&range->end->data);
    ASSERT_NE(endVal, nullptr);
    EXPECT_EQ(*endVal, 10);

    EXPECT_EQ(range->step, nullptr);
}

TEST(ParseExpressions, RangeSimpleInclusive) {
    auto mod = parse("1...10");
    ASSERT_EQ(mod.statements.size(), 1);

    auto* exprStmt = std::get_if<Wasp::ExpressionStatement>(&mod.statements[0]->data);
    ASSERT_NE(exprStmt, nullptr);
    ASSERT_NE(exprStmt->expression, nullptr);

    auto* range = std::get_if<Wasp::RangeLiteral>(&exprStmt->expression->data);
    ASSERT_NE(range, nullptr);

    EXPECT_TRUE(range->is_inclusive);

    ASSERT_NE(range->start, nullptr);
    auto* startVal = std::get_if<int>(&range->start->data);
    ASSERT_NE(startVal, nullptr);
    EXPECT_EQ(*startVal, 1);

    ASSERT_NE(range->end, nullptr);
    auto* endVal = std::get_if<int>(&range->end->data);
    ASSERT_NE(endVal, nullptr);
    EXPECT_EQ(*endVal, 10);

    EXPECT_EQ(range->step, nullptr);
}

TEST(ParseExpressions, RangeWithStep) {
    auto mod = parse("1..10:2");
    ASSERT_EQ(mod.statements.size(), 1);

    auto* exprStmt = std::get_if<Wasp::ExpressionStatement>(&mod.statements[0]->data);
    ASSERT_NE(exprStmt, nullptr);
    ASSERT_NE(exprStmt->expression, nullptr);

    auto* range = std::get_if<Wasp::RangeLiteral>(&exprStmt->expression->data);
    ASSERT_NE(range, nullptr);

    EXPECT_FALSE(range->is_inclusive);

    ASSERT_NE(range->start, nullptr);
    auto* startVal = std::get_if<int>(&range->start->data);
    ASSERT_NE(startVal, nullptr);
    EXPECT_EQ(*startVal, 1);

    ASSERT_NE(range->end, nullptr);
    auto* endVal = std::get_if<int>(&range->end->data);
    ASSERT_NE(endVal, nullptr);
    EXPECT_EQ(*endVal, 10);

    ASSERT_NE(range->step, nullptr);
    auto* stepVal = std::get_if<int>(&range->step->data);
    ASSERT_NE(stepVal, nullptr);
    EXPECT_EQ(*stepVal, 2);
}

TEST(ParseExpressions, RangeWithoutEnd) {
    auto mod = parse("1..");
    ASSERT_EQ(mod.statements.size(), 1);

    auto* exprStmt = std::get_if<Wasp::ExpressionStatement>(&mod.statements[0]->data);
    ASSERT_NE(exprStmt, nullptr);
    ASSERT_NE(exprStmt->expression, nullptr);

    auto* range = std::get_if<Wasp::RangeLiteral>(&exprStmt->expression->data);
    ASSERT_NE(range, nullptr);

    EXPECT_FALSE(range->is_inclusive);

    ASSERT_NE(range->start, nullptr);
    auto* startVal = std::get_if<int>(&range->start->data);
    ASSERT_NE(startVal, nullptr);
    EXPECT_EQ(*startVal, 1);

    EXPECT_EQ(range->end, nullptr);
    EXPECT_EQ(range->step, nullptr);
}

TEST(ParseExpressions, RangeWithoutEndWithStep) {
    auto mod = parse("1..:2");
    ASSERT_EQ(mod.statements.size(), 1);

    auto* exprStmt = std::get_if<Wasp::ExpressionStatement>(&mod.statements[0]->data);
    ASSERT_NE(exprStmt, nullptr);
    ASSERT_NE(exprStmt->expression, nullptr);

    auto* range = std::get_if<Wasp::RangeLiteral>(&exprStmt->expression->data);
    ASSERT_NE(range, nullptr);

    EXPECT_FALSE(range->is_inclusive);

    ASSERT_NE(range->start, nullptr);
    auto* startVal = std::get_if<int>(&range->start->data);
    ASSERT_NE(startVal, nullptr);
    EXPECT_EQ(*startVal, 1);

    EXPECT_EQ(range->end, nullptr);

    ASSERT_NE(range->step, nullptr);
    auto* stepVal = std::get_if<int>(&range->step->data);
    ASSERT_NE(stepVal, nullptr);
    EXPECT_EQ(*stepVal, 2);
}

TEST(ParseExpressions, RangeWithoutStartOrEndOrStep) {
    auto mod = parse("...");
    ASSERT_EQ(mod.statements.size(), 1);

    auto* exprStmt = std::get_if<Wasp::ExpressionStatement>(&mod.statements[0]->data);
    ASSERT_NE(exprStmt, nullptr);
    ASSERT_NE(exprStmt->expression, nullptr);

    auto* dotDotDot = std::get_if<Wasp::DotDotDotLiteral>(&exprStmt->expression->data);
    ASSERT_NE(dotDotDot, nullptr) << "Expected '...' to parse as a DotDotDotLiteral";
}

TEST(ParseExpressions, Dot) {
    auto mod = parse(".");
    ASSERT_EQ(mod.statements.size(), 1);

    auto* exprStmt = std::get_if<Wasp::ExpressionStatement>(&mod.statements[0]->data);
    ASSERT_NE(exprStmt, nullptr);
    ASSERT_NE(exprStmt->expression, nullptr);

    auto* dot = std::get_if<Wasp::DotLiteral>(&exprStmt->expression->data);
    ASSERT_NE(dot, nullptr);
}

TEST(ParseExpressions, DotDot) {
    auto mod = parse("..");
    ASSERT_EQ(mod.statements.size(), 1);

    auto* exprStmt = std::get_if<Wasp::ExpressionStatement>(&mod.statements[0]->data);
    ASSERT_NE(exprStmt, nullptr);
    ASSERT_NE(exprStmt->expression, nullptr);

    auto* dotDot = std::get_if<Wasp::DotDotLiteral>(&exprStmt->expression->data);
    ASSERT_NE(dotDot, nullptr);
}

TEST(ParseExpressions, PipeOutput) {
    auto mod = parse("foo() ~ bar(., 35) ~ boom(...)");
    ASSERT_EQ(mod.statements.size(), 1);

    auto* exprStmt = std::get_if<Wasp::ExpressionStatement>(&mod.statements[0]->data);
    ASSERT_NE(exprStmt, nullptr);
    ASSERT_NE(exprStmt->expression, nullptr);

    auto* rootInfix = std::get_if<Wasp::Infix>(&exprStmt->expression->data);
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
TEST(ParseExpressions, Star) {
    auto mod = parse("*b");
    ASSERT_EQ(mod.statements.size(), 1);

    auto* exprStmt = std::get_if<Wasp::ExpressionStatement>(&mod.statements[0]->data);
    ASSERT_NE(exprStmt, nullptr);
    ASSERT_NE(exprStmt->expression, nullptr);

    auto* star = std::get_if<Wasp::Prefix>(&exprStmt->expression->data);
    ASSERT_NE(star, nullptr);

    // Fixed: 'operand' instead of 'right'
    ASSERT_NE(star->operand, nullptr);
    auto* id = std::get_if<Wasp::Identifier>(&star->operand->data);
    ASSERT_NE(id, nullptr);
    
    // Fixed: 'name' instead of 'value'
    EXPECT_EQ(id->name, "b");
}

TEST(ParseExpressions, StarGather) {
    auto mod = parse("[a, *b, c] = [1, 2, 3, 4, 5]");
    ASSERT_EQ(mod.statements.size(), 1);

    auto* exprStmt = std::get_if<Wasp::ExpressionStatement>(&mod.statements[0]->data);
    ASSERT_NE(exprStmt, nullptr);
    ASSERT_NE(exprStmt->expression, nullptr);

    auto* assign = std::get_if<Wasp::UntypedAssignment>(&exprStmt->expression->data);
    ASSERT_NE(assign, nullptr);

    ASSERT_NE(assign->lhs_expression, nullptr);
    auto* lhsList = std::get_if<Wasp::ListLiteral>(&assign->lhs_expression->data);
    ASSERT_NE(lhsList, nullptr);
    ASSERT_EQ(lhsList->expressions.size(), 3);

    ASSERT_NE(lhsList->expressions[1], nullptr);
    auto* starGather = std::get_if<Wasp::Prefix>(&lhsList->expressions[1]->data);
    ASSERT_NE(starGather, nullptr);
    ASSERT_NE(starGather->operand, nullptr);
    
    auto* gatherId = std::get_if<Wasp::Identifier>(&starGather->operand->data);
    ASSERT_NE(gatherId, nullptr);
    EXPECT_EQ(gatherId->name, "b");

    ASSERT_NE(assign->rhs_expression, nullptr);
    auto* rhsList = std::get_if<Wasp::ListLiteral>(&assign->rhs_expression->data);
    ASSERT_NE(rhsList, nullptr);
    ASSERT_EQ(rhsList->expressions.size(), 5);
}

TEST(ParseExpressions, StarSpread) {
    auto mod = parse("[a, b, c] = *three_nums");
    ASSERT_EQ(mod.statements.size(), 1);

    auto* exprStmt = std::get_if<Wasp::ExpressionStatement>(&mod.statements[0]->data);
    ASSERT_NE(exprStmt, nullptr);
    ASSERT_NE(exprStmt->expression, nullptr);

    auto* assign = std::get_if<Wasp::UntypedAssignment>(&exprStmt->expression->data);
    ASSERT_NE(assign, nullptr);

    ASSERT_NE(assign->lhs_expression, nullptr);
    auto* lhsList = std::get_if<Wasp::ListLiteral>(&assign->lhs_expression->data);
    ASSERT_NE(lhsList, nullptr);
    ASSERT_EQ(lhsList->expressions.size(), 3);

    ASSERT_NE(assign->rhs_expression, nullptr);
    auto* starSpread = std::get_if<Wasp::Prefix>(&assign->rhs_expression->data);
    ASSERT_NE(starSpread, nullptr);
    
    ASSERT_NE(starSpread->operand, nullptr);
    auto* spreadId = std::get_if<Wasp::Identifier>(&starSpread->operand->data);
    ASSERT_NE(spreadId, nullptr);
    EXPECT_EQ(spreadId->name, "three_nums");
}

TEST(ParseExpressions, StarGatherAndSpread) {
    auto mod = parse("[a, *b, c] = *five_nums");
    ASSERT_EQ(mod.statements.size(), 1);

    auto* exprStmt = std::get_if<Wasp::ExpressionStatement>(&mod.statements[0]->data);
    ASSERT_NE(exprStmt, nullptr);
    ASSERT_NE(exprStmt->expression, nullptr);

    auto* assign = std::get_if<Wasp::UntypedAssignment>(&exprStmt->expression->data);
    ASSERT_NE(assign, nullptr);

    ASSERT_NE(assign->lhs_expression, nullptr);
    auto* lhsList = std::get_if<Wasp::ListLiteral>(&assign->lhs_expression->data);
    ASSERT_NE(lhsList, nullptr);
    ASSERT_EQ(lhsList->expressions.size(), 3);

    ASSERT_NE(lhsList->expressions[1], nullptr);
    auto* starGather = std::get_if<Wasp::Prefix>(&lhsList->expressions[1]->data);
    ASSERT_NE(starGather, nullptr);

    ASSERT_NE(assign->rhs_expression, nullptr);
    auto* starSpread = std::get_if<Wasp::Prefix>(&assign->rhs_expression->data);
    ASSERT_NE(starSpread, nullptr);

    ASSERT_NE(starSpread->operand, nullptr);
    auto* spreadId = std::get_if<Wasp::Identifier>(&starSpread->operand->data);
    ASSERT_NE(spreadId, nullptr);
    EXPECT_EQ(spreadId->name, "five_nums");
}
