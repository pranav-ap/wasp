#include "token.h"
#include "lexer.h"
#include "parser.h"
#include "test_utils.h"
#include <gtest/gtest.h>


TEST(ParseBranching, TernaryExpression) {
    auto mod = parse("if true then 1 else 2");
    ASSERT_EQ(mod.statements.size(), 1);
    
    auto* exprStmt = std::get_if<Wasp::ExpressionStatement>(&mod.statements[0]->data);
    ASSERT_NE(exprStmt, nullptr) << "Expected an ExpressionStatement";

    ASSERT_NE(exprStmt->expression, nullptr) << "Expression pointer is null";
    ASSERT_TRUE(exprStmt->expression->is<Wasp::IfTernaryBranch>());
    
    const auto& ternary = exprStmt->expression->as<Wasp::IfTernaryBranch>();
    
    ASSERT_NE(ternary.test, nullptr) << "Ternary test condition is null";
    ASSERT_TRUE(ternary.test->is<bool>());
    EXPECT_EQ(ternary.test->as<bool>(), true);
    
    ASSERT_NE(ternary.true_expression, nullptr) << "Ternary true_expression is null";
    ASSERT_TRUE(ternary.true_expression->is<int>());
    EXPECT_EQ(ternary.true_expression->as<int>(), 1);
    
    ASSERT_NE(ternary.alternative, nullptr) << "Ternary alternative is null";
    ASSERT_TRUE(ternary.alternative->is<Wasp::ElseTernaryBranch>());
    
    const auto& elseBranch = ternary.alternative->as<Wasp::ElseTernaryBranch>();
    
    ASSERT_NE(elseBranch.expression, nullptr) << "Else branch inner expression is null";
    ASSERT_TRUE(elseBranch.expression->is<int>());
    EXPECT_EQ(elseBranch.expression->as<int>(), 2);
}

TEST(ParseBranching, TernaryLetExpression) {
    auto mod = parse("if let x = 1 then 1 else 2");
    ASSERT_EQ(mod.statements.size(), 1);

    auto* exprStmt = std::get_if<Wasp::ExpressionStatement>(&mod.statements[0]->data);
    ASSERT_NE(exprStmt, nullptr) << "Expected an ExpressionStatement";
    ASSERT_NE(exprStmt->expression, nullptr) << "Expression pointer is null";
    ASSERT_TRUE(exprStmt->expression->is<Wasp::IfTernaryBranch>());

    const auto& ternary = exprStmt->expression->as<Wasp::IfTernaryBranch>();
    ASSERT_NE(ternary.test, nullptr) << "Ternary test condition is null";
    ASSERT_TRUE(ternary.test->is<Wasp::VariableDefinitionExpression>());

    const auto& letExpr = ternary.test->as<Wasp::VariableDefinitionExpression>();
    
    ASSERT_NE(letExpr.assignment, nullptr);
    ASSERT_TRUE(letExpr.assignment->is<Wasp::UntypedAssignment>());
    
    const auto& assignment = letExpr.assignment->as<Wasp::UntypedAssignment>();

    ASSERT_NE(assignment.lhs_expression, nullptr);
    ASSERT_TRUE(assignment.lhs_expression->is<Wasp::Identifier>());
    EXPECT_EQ(assignment.lhs_expression->as<Wasp::Identifier>().name, "x");
    
    ASSERT_NE(assignment.rhs_expression, nullptr);
    ASSERT_TRUE(assignment.rhs_expression->is<int>());
    EXPECT_EQ(assignment.rhs_expression->as<int>(), 1);
}

TEST(ParseBranching, IfBlock) {
    auto mod = parse(R"(
if true then
    1 
)");
    ASSERT_EQ(mod.statements.size(), 1);

    auto* ifBranch = std::get_if<Wasp::IfBranch>(&mod.statements[0]->data);
    ASSERT_NE(ifBranch, nullptr);

    // 1. Check Condition
    ASSERT_NE(ifBranch->test, nullptr);
    auto* testVal = std::get_if<bool>(&ifBranch->test->data);
    ASSERT_NE(testVal, nullptr);
    EXPECT_EQ(*testVal, true);

    // 2. Check Body
    ASSERT_EQ(ifBranch->body.size(), 1);
    auto* innerExprStmt = std::get_if<Wasp::ExpressionStatement>(&ifBranch->body[0]->data);
    ASSERT_NE(innerExprStmt, nullptr);
    
    ASSERT_NE(innerExprStmt->expression, nullptr);
    auto* bodyVal = std::get_if<int>(&innerExprStmt->expression->data);
    ASSERT_NE(bodyVal, nullptr);
    EXPECT_EQ(*bodyVal, 1);

    EXPECT_FALSE(ifBranch->alternative.has_value());
}
TEST(ParseBranching, IfElseBlock) {
    auto mod = parse(R"(
if true then
    1
else
    2
)");

    ASSERT_EQ(mod.statements.size(), 1);

    auto* ifBranch = std::get_if<Wasp::IfBranch>(&mod.statements[0]->data);
    ASSERT_NE(ifBranch, nullptr);

    // 1. Check Condition
    ASSERT_NE(ifBranch->test, nullptr);
    auto* testVal = std::get_if<bool>(&ifBranch->test->data);
    ASSERT_NE(testVal, nullptr);
    EXPECT_EQ(*testVal, true);

    // 2. Check If Body
    ASSERT_EQ(ifBranch->body.size(), 1);
    auto* innerExprStmt = std::get_if<Wasp::ExpressionStatement>(&ifBranch->body[0]->data);
    ASSERT_NE(innerExprStmt, nullptr);
    
    ASSERT_NE(innerExprStmt->expression, nullptr);
    auto* ifBodyVal = std::get_if<int>(&innerExprStmt->expression->data);
    ASSERT_NE(ifBodyVal, nullptr);
    EXPECT_EQ(*ifBodyVal, 1);

    // 3. Extract Else Branch
    ASSERT_TRUE(ifBranch->alternative.has_value());
    auto elseBranchStmt = ifBranch->alternative.value();
    ASSERT_NE(elseBranchStmt, nullptr);
    
    auto* elseBlock = std::get_if<Wasp::ElseBranch>(&elseBranchStmt->data);
    ASSERT_NE(elseBlock, nullptr);
    ASSERT_EQ(elseBlock->body.size(), 1);
    
    // 4. Check Else Body
    auto* elseInnerExprStmt = std::get_if<Wasp::ExpressionStatement>(&elseBlock->body[0]->data);
    ASSERT_NE(elseInnerExprStmt, nullptr);
    
    ASSERT_NE(elseInnerExprStmt->expression, nullptr);
    auto* elseBodyVal = std::get_if<int>(&elseInnerExprStmt->expression->data);
    ASSERT_NE(elseBodyVal, nullptr);
    EXPECT_EQ(*elseBodyVal, 2);
}


TEST(ParseBranching, IfElifElseBlock) {
    auto mod = parse(R"(
if x == 25 then
    pass
elif x == 30 then
    pass
else
    pass
)");

    ASSERT_EQ(mod.statements.size(), 1);

    auto* ifBranch = std::get_if<Wasp::IfBranch>(&mod.statements[0]->data);
    ASSERT_NE(ifBranch, nullptr);

    ASSERT_NE(ifBranch->test, nullptr);
    auto* infix = std::get_if<Wasp::Infix>(&ifBranch->test->data);
    ASSERT_NE(infix, nullptr);

    ASSERT_NE(infix->left, nullptr);
    auto* leftId = std::get_if<Wasp::Identifier>(&infix->left->data);
    ASSERT_NE(leftId, nullptr);
    EXPECT_EQ(leftId->name, "x");

    EXPECT_EQ(infix->op.type, Wasp::TokenType::EQUAL_EQUAL);

    ASSERT_NE(infix->right, nullptr);
    auto* rightInt = std::get_if<int>(&infix->right->data);
    ASSERT_NE(rightInt, nullptr);
    EXPECT_EQ(*rightInt, 25);

    ASSERT_EQ(ifBranch->body.size(), 1);
    auto* passStmt = std::get_if<Wasp::Pass>(&ifBranch->body[0]->data);
    ASSERT_NE(passStmt, nullptr);

    ASSERT_TRUE(ifBranch->alternative.has_value());
    auto elifStmt = ifBranch->alternative.value();
    ASSERT_NE(elifStmt, nullptr);

    auto* elifBranch = std::get_if<Wasp::IfBranch>(&elifStmt->data);
    ASSERT_NE(elifBranch, nullptr) << "Expected elif to be parsed as a nested IfBranch";

    ASSERT_NE(elifBranch->test, nullptr);
    auto* elifInfix = std::get_if<Wasp::Infix>(&elifBranch->test->data);
    ASSERT_NE(elifInfix, nullptr);
    
    ASSERT_NE(elifInfix->left, nullptr);
    auto* elifLeftId = std::get_if<Wasp::Identifier>(&elifInfix->left->data);
    ASSERT_NE(elifLeftId, nullptr);
    EXPECT_EQ(elifLeftId->name, "x");

    EXPECT_EQ(elifInfix->op.type, Wasp::TokenType::EQUAL_EQUAL); 

    ASSERT_NE(elifInfix->right, nullptr);
    auto* elifRightInt = std::get_if<int>(&elifInfix->right->data);
    ASSERT_NE(elifRightInt, nullptr);
    EXPECT_EQ(*elifRightInt, 30);

    ASSERT_EQ(elifBranch->body.size(), 1);
    auto* elifPassStmt = std::get_if<Wasp::Pass>(&elifBranch->body[0]->data);
    ASSERT_NE(elifPassStmt, nullptr);

    ASSERT_TRUE(elifBranch->alternative.has_value());
    auto elseStmt = elifBranch->alternative.value();
    ASSERT_NE(elseStmt, nullptr);

    auto* elseBranch = std::get_if<Wasp::ElseBranch>(&elseStmt->data);
    ASSERT_NE(elseBranch, nullptr) << "Expected final alternative to be an ElseBranch";
    
    ASSERT_EQ(elseBranch->body.size(), 1);
    auto* elsePassStmt = std::get_if<Wasp::Pass>(&elseBranch->body[0]->data);
    ASSERT_NE(elsePassStmt, nullptr);
}

