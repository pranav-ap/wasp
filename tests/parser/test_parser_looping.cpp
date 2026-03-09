#include "Token.h"
#include "Lexer.h"
#include "Parser.h"
#include "test_utils.h"
#include <gtest/gtest.h>


TEST(ParseLooping, WhileSingle) {
    auto mod = parse(R"(while x < 10 do x = x + 1)");
    
    auto& stmt = check<Wasp::SimpleLoop>(mod.statements[0]);
    
    {
        auto& condInfix = check<Wasp::Infix>(stmt.condition);
        auto& left = check<Wasp::Identifier>(condInfix.left);
        EXPECT_EQ(left.name, "x");
        auto& right = check<int>(condInfix.right);
        EXPECT_EQ(right, 10);
        EXPECT_EQ(condInfix.op.type, Wasp::TokenType::LESSER_THAN);
    }

    {
        auto& body = check<Wasp::ExpressionStatement>(stmt.body[0]);
        auto& assign = check<Wasp::UntypedAssignment>(body.expression);
        
        {
            auto& lhs = check<Wasp::Identifier>(assign.lhs_expression);
            EXPECT_EQ(lhs.name, "x");
        }

        {
            auto& rhs = check<Wasp::Infix>(assign.rhs_expression);
            EXPECT_EQ(rhs.op.type, Wasp::TokenType::PLUS);

            {
                auto& inner_left = check<Wasp::Identifier>(rhs.left);
                EXPECT_EQ(inner_left.name, "x");

                auto& inner_right = check<int>(rhs.right);
                EXPECT_EQ(inner_right, 1);
            }
        }
    }
}

TEST(ParseLooping, WhileBlock) {
    auto mod = parse(R"(
while x < 10 do 
    x = x + 1
    y = 1
)");

    ASSERT_EQ(mod.statements.size(), 1);

    auto& stmt = check<Wasp::SimpleLoop>(mod.statements[0]);
    auto& body = stmt.body;
    ASSERT_EQ(body.size(), 2);

    {
        auto& exprStmt = check<Wasp::ExpressionStatement>(body[0]);
        auto& assign = check<Wasp::UntypedAssignment>(exprStmt.expression);
    }

    {
        auto& exprStmt = check<Wasp::ExpressionStatement>(body[1]);
        auto& assign = check<Wasp::UntypedAssignment>(exprStmt.expression);
    }
}

TEST(ParseLooping, Continue) {
    auto mod = parse(R"(
while x < 10 do 
    x = x + 1
    continue
)");

    auto& stmt = check<Wasp::SimpleLoop>(mod.statements[0]);
    auto& body = stmt.body;
    ASSERT_EQ(body.size(), 2);

    auto& exprStmt = check<Wasp::ExpressionStatement>(body[0]);
    auto& assign = check<Wasp::UntypedAssignment>(exprStmt.expression);

    auto& ctrlStmt = check<Wasp::LoopControl>(body[1]);
    EXPECT_EQ(ctrlStmt.type, Wasp::TokenType::CONTINUE);
}

TEST(ParseLooping, ContinueWithExpression) {
    auto mod = parse(R"(
while x < 10 do 
    x = x + 1
    continue x
)"); 

    auto& stmt = check<Wasp::SimpleLoop>(mod.statements[0]);
    auto& body = stmt.body;
    ASSERT_EQ(body.size(), 2);

    auto& exprStmt = check<Wasp::ExpressionStatement>(body[0]);
    auto& assign = check<Wasp::UntypedAssignment>(exprStmt.expression);

    auto& ctrlStmt = check<Wasp::LoopControl>(body[1]);
    EXPECT_EQ(ctrlStmt.type, Wasp::TokenType::CONTINUE);
}

