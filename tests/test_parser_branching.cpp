#include "token.h"
#include "lexer.h"
#include "parser.h"
#include "test_utils.h"
#include <gtest/gtest.h>


TEST(ParserTestSuite, TernaryExpression) {
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

TEST(ParserTestSuite, TernaryLetExpression) {
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

TEST(ParserTestSuite, IfBlock) {
    auto mod = parse(R"(
if true then
    1 
)");
    ASSERT_EQ(mod.statements.size(), 1);

    auto* ifBranch = std::get_if<Wasp::IfBranch>(&mod.statements[0]->data);
    ASSERT_NE(ifBranch, nullptr) << "Expected an IfBranch statement";

    // Safely check the 'test' expression
    ASSERT_NE(ifBranch->test, nullptr) << "If condition pointer is null";
    ASSERT_TRUE(ifBranch->test->template is<bool>());
    EXPECT_EQ(ifBranch->test->template as<bool>(), true);

    // Safely check the 'body' block
    ASSERT_EQ(ifBranch->body.size(), 1);
    auto* innerExpr = std::get_if<Wasp::ExpressionStatement>(&ifBranch->body[0]->data);
    ASSERT_NE(innerExpr, nullptr) << "Expected an ExpressionStatement inside the If block";
    
    // Safely check the expression inside the body
    ASSERT_NE(innerExpr->expression, nullptr) << "Body expression pointer is null";
    ASSERT_TRUE(innerExpr->expression->template is<int>());
    EXPECT_EQ(innerExpr->expression->template as<int>(), 1);

    // Verify there is no 'else' alternative
    EXPECT_FALSE(ifBranch->alternative.has_value());
}

// TEST(ParserTestSuite, IfElseBlock) {
//     auto mod = parse(R"(
// if true then
//     1
// else
//     2
// )");

//     ASSERT_EQ(mod.statements.size(), 1);

//     auto* ifBranch = std::get_if<Wasp::IfBranch>(&mod.statements[0]->data);
//     ASSERT_NE(ifBranch, nullptr);

//     ASSERT_TRUE(ifBranch->test->is<bool>());
//     EXPECT_EQ(ifBranch->test->as<bool>(), true);

//     ASSERT_EQ(ifBranch->body.size(), 1);
//     auto* innerExpr = std::get_if<Wasp::ExpressionStatement>(&ifBranch->body[0]->data);
//     ASSERT_NE(innerExpr, nullptr);
//     ASSERT_TRUE(innerExpr->expression->is<int>());
//     EXPECT_EQ(innerExpr->expression->as<int>(), 1);

//     ASSERT_TRUE(ifBranch->alternative.has_value());
//     const auto& elseBranch = ifBranch->alternative.value();
//     ASSERT_TRUE(elseBranch.is<Wasp::ElseBranch>());
    
//     const auto& elseBlock = elseBranch.as<Wasp::ElseBranch>();
//     ASSERT_EQ(elseBlock.body.size(), 1);
    
//     auto* elseInnerExpr = std::get_if<Wasp::ExpressionStatement>(&elseBlock.body[0]->data);
//     ASSERT_NE(elseInnerExpr, nullptr);
//     ASSERT_TRUE(elseInnerExpr->expression->is<int>());
//     EXPECT_EQ(elseInnerExpr->expression->as<int>(), 2);
// }


// TEST(ParserTestSuite, IfElifElseBlock) {
//     auto mod = parse(R"(
// if x == 25 then
//     pass
// elif x == 30 then
//     pass
// else
//     pass
// )");

//     ASSERT_EQ(mod.statements.size(), 1);

//     // 1. Root IF
//     auto* ifBranch = std::get_if<Wasp::IfBranch>(&mod.statements[0]->data);
//     ASSERT_NE(ifBranch, nullptr);

//     ASSERT_NE(ifBranch->test, nullptr);
//     ASSERT_TRUE(ifBranch->test->is<Wasp::Infix>());
//     const auto& infix = ifBranch->test->as<Wasp::Infix>();

//     ASSERT_TRUE(infix.left->is<Wasp::Identifier>());
//     EXPECT_EQ(infix.left->as<Wasp::Identifier>().name, "x");
//     EXPECT_EQ(infix.op.type, Wasp::TokenType::EQUAL_EQUAL); // Assuming you have TokenType included

//     ASSERT_TRUE(infix.right->is<int>());
//     EXPECT_EQ(infix.right->as<int>(), 25);

//     ASSERT_EQ(ifBranch->body.size(), 1);
//     auto* passStmt = std::get_if<Wasp::Pass>(&ifBranch->body[0]->data);
//     ASSERT_NE(passStmt, nullptr);

//     // 2. ELIF (Parsed as a nested IfBranch in the alternative)
//     ASSERT_TRUE(ifBranch->alternative.has_value());
//     auto elifStmt = ifBranch->alternative.value();
//     ASSERT_NE(elifStmt, nullptr);

//     auto* elifBranch = std::get_if<Wasp::IfBranch>(&elifStmt->data);
//     ASSERT_NE(elifBranch, nullptr) << "Expected elif to be parsed as a nested IfBranch";

//     ASSERT_NE(elifBranch->test, nullptr);
//     ASSERT_TRUE(elifBranch->test->is<Wasp::Infix>());
//     const auto& elifInfix = elifBranch->test->as<Wasp::Infix>();
    
//     ASSERT_TRUE(elifInfix.left->is<Wasp::Identifier>());
//     EXPECT_EQ(elifInfix.left->as<Wasp::Identifier>().name, "x");
//     EXPECT_EQ(elifInfix.op.type, Wasp::TokenType::EQUAL_EQUAL); 

//     ASSERT_TRUE(elifInfix.right->is<int>());
//     EXPECT_EQ(elifInfix.right->as<int>(), 30);

//     ASSERT_EQ(elifBranch->body.size(), 1);
//     auto* elifPassStmt = std::get_if<Wasp::Pass>(&elifBranch->body[0]->data);
//     ASSERT_NE(elifPassStmt, nullptr);

//     // 3. ELSE (Parsed as an ElseBranch in the elif's alternative)
//     ASSERT_TRUE(elifBranch->alternative.has_value());
//     auto elseStmt = elifBranch->alternative.value();
//     ASSERT_NE(elseStmt, nullptr);

//     auto* elseBranch = std::get_if<Wasp::ElseBranch>(&elseStmt->data);
//     ASSERT_NE(elseBranch, nullptr) << "Expected final alternative to be an ElseBranch";
    
//     ASSERT_EQ(elseBranch->body.size(), 1);
//     auto* elsePassStmt = std::get_if<Wasp::Pass>(&elseBranch->body[0]->data);
//     ASSERT_NE(elsePassStmt, nullptr);
// }
