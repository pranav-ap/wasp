#include "token.h"
#include "lexer.h"
#include "parser.h"
#include "test_utils.h"
#include <gtest/gtest.h>

TEST(ParseLooping, WhileSingle) {
    auto mod = parse(R"(while x < 10 do x = x + 1)");
    ASSERT_EQ(mod.statements.size(), 1);

    auto* loop = std::get_if<Wasp::SimpleLoop>(&mod.statements[0]->data);
    ASSERT_NE(loop, nullptr);

    ASSERT_NE(loop->condition, nullptr);
    auto* condInfix = std::get_if<Wasp::Infix>(&loop->condition->data);
    ASSERT_NE(condInfix, nullptr);
    
    auto* condLeft = std::get_if<Wasp::Identifier>(&condInfix->left->data);
    ASSERT_NE(condLeft, nullptr);
    EXPECT_EQ(condLeft->name, "x");
    
    auto* condRight = std::get_if<int>(&condInfix->right->data);
    ASSERT_NE(condRight, nullptr);
    EXPECT_EQ(*condRight, 10);

    ASSERT_EQ(loop->body.size(), 1);
    auto* exprStmt = std::get_if<Wasp::ExpressionStatement>(&loop->body[0]->data);
    ASSERT_NE(exprStmt, nullptr);
    
    ASSERT_NE(exprStmt->expression, nullptr);
    auto* assign = std::get_if<Wasp::UntypedAssignment>(&exprStmt->expression->data);
    ASSERT_NE(assign, nullptr);
    
    ASSERT_NE(assign->lhs_expression, nullptr);
    auto* assignLeft = std::get_if<Wasp::Identifier>(&assign->lhs_expression->data);
    ASSERT_NE(assignLeft, nullptr);
    EXPECT_EQ(assignLeft->name, "x");
}

TEST(ParseLooping, WhileBlock) {
    auto mod = parse(R"(
while x < 10 do 
    x = x + 1
    y = 1
)");

    ASSERT_EQ(mod.statements.size(), 1);

    auto* loop = std::get_if<Wasp::SimpleLoop>(&mod.statements[0]->data);
    ASSERT_NE(loop, nullptr);

    ASSERT_EQ(loop->body.size(), 2);

    auto* exprStmt1 = std::get_if<Wasp::ExpressionStatement>(&loop->body[0]->data);
    ASSERT_NE(exprStmt1, nullptr);
    auto* assign1 = std::get_if<Wasp::UntypedAssignment>(&exprStmt1->expression->data);
    ASSERT_NE(assign1, nullptr);
    auto* lhs1 = std::get_if<Wasp::Identifier>(&assign1->lhs_expression->data);
    ASSERT_NE(lhs1, nullptr);
    EXPECT_EQ(lhs1->name, "x");

    auto* exprStmt2 = std::get_if<Wasp::ExpressionStatement>(&loop->body[1]->data);
    ASSERT_NE(exprStmt2, nullptr);
    auto* assign2 = std::get_if<Wasp::UntypedAssignment>(&exprStmt2->expression->data);
    ASSERT_NE(assign2, nullptr);
    auto* lhs2 = std::get_if<Wasp::Identifier>(&assign2->lhs_expression->data);
    ASSERT_NE(lhs2, nullptr);
    EXPECT_EQ(lhs2->name, "y");
    
    auto* rhs2 = std::get_if<int>(&assign2->rhs_expression->data);
    ASSERT_NE(rhs2, nullptr);
    EXPECT_EQ(*rhs2, 1);
}

TEST(ParseLooping, Continue) {
    auto mod = parse(R"(
while x < 10 do 
    x = x + 1
    continue
)");

    ASSERT_EQ(mod.statements.size(), 1);

    auto* loop = std::get_if<Wasp::SimpleLoop>(&mod.statements[0]->data);
    ASSERT_NE(loop, nullptr);
    ASSERT_EQ(loop->body.size(), 2);

    auto* exprStmt = std::get_if<Wasp::ExpressionStatement>(&loop->body[0]->data);
    ASSERT_NE(exprStmt, nullptr);

    auto* ctrlStmt = std::get_if<Wasp::LoopControl>(&loop->body[1]->data);
    ASSERT_NE(ctrlStmt, nullptr);
    
    EXPECT_EQ(ctrlStmt->type, Wasp::TokenType::CONTINUE); 
}

TEST(ParseLooping, ForSingle) {
    auto mod = parse(R"(for x in [1, 2, 3] do x = x + 1)");

    ASSERT_EQ(mod.statements.size(), 1);

    auto* loop = std::get_if<Wasp::ForInLoop>(&mod.statements[0]->data);
    ASSERT_NE(loop, nullptr);

    ASSERT_NE(loop->lhs, nullptr);
    auto* lhsId = std::get_if<Wasp::Identifier>(&loop->lhs->data);
    ASSERT_NE(lhsId, nullptr);
    EXPECT_EQ(lhsId->name, "x");

    ASSERT_NE(loop->iterable_expression, nullptr);
    auto* listLit = std::get_if<Wasp::ListLiteral>(&loop->iterable_expression->data);
    ASSERT_NE(listLit, nullptr);
    ASSERT_EQ(listLit->expressions.size(), 3);
    
    auto* firstListEl = std::get_if<int>(&listLit->expressions[0]->data);
    ASSERT_NE(firstListEl, nullptr);
    EXPECT_EQ(*firstListEl, 1);

    ASSERT_EQ(loop->body.size(), 1);
    
    auto* exprStmt = std::get_if<Wasp::ExpressionStatement>(&loop->body[0]->data);
    ASSERT_NE(exprStmt, nullptr);
    auto* assign = std::get_if<Wasp::UntypedAssignment>(&exprStmt->expression->data);
    ASSERT_NE(assign, nullptr);
}

TEST(ParseLooping, ForBlock) {
    auto mod = parse(R"(
for x in [1, 2, 3] do 
    x = x + 1
    y = 1
)");

    ASSERT_EQ(mod.statements.size(), 1);

    auto* loop = std::get_if<Wasp::ForInLoop>(&mod.statements[0]->data);
    ASSERT_NE(loop, nullptr);

    ASSERT_NE(loop->iterable_expression, nullptr);
    auto* listLit = std::get_if<Wasp::ListLiteral>(&loop->iterable_expression->data);
    ASSERT_NE(listLit, nullptr);
    ASSERT_EQ(listLit->expressions.size(), 3);

    ASSERT_EQ(loop->body.size(), 2);

    auto* exprStmt2 = std::get_if<Wasp::ExpressionStatement>(&loop->body[1]->data);
    ASSERT_NE(exprStmt2, nullptr);
    auto* assign2 = std::get_if<Wasp::UntypedAssignment>(&exprStmt2->expression->data);
    ASSERT_NE(assign2, nullptr);
    auto* lhs2 = std::get_if<Wasp::Identifier>(&assign2->lhs_expression->data);
    ASSERT_NE(lhs2, nullptr);
    EXPECT_EQ(lhs2->name, "y");
}
