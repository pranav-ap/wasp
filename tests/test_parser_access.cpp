#include "token.h"
#include "lexer.h"
#include "parser.h"
#include "test_utils.h"
#include <gtest/gtest.h>


TEST(ParseExpressions, MemberAccessNested) {
    auto mod = parse(R"(Animal.Dog.GermanShepherd)");
    ASSERT_EQ(mod.statements.size(), 1);

    // FIX: Renamed stmt_wrapper to expr_stmt to match the usage below
    auto& expr_stmt = check<Wasp::ExpressionStatement>(mod.statements[0]);
    
    // Top Level: [Left] . GermanShepherd
    {
        auto& top_infix = check<Wasp::Infix>(expr_stmt.expression);
        EXPECT_EQ(top_infix.op.type, Wasp::TokenType::DOT);

        auto& right_id = check<Wasp::Identifier>(top_infix.right);
        EXPECT_EQ(right_id.name, "GermanShepherd");

        // Nested Level: Animal . Dog
        {
            auto& nested_infix = check<Wasp::Infix>(top_infix.left);
            EXPECT_EQ(nested_infix.op.type, Wasp::TokenType::DOT);

            auto& left_id = check<Wasp::Identifier>(nested_infix.left);
            EXPECT_EQ(left_id.name, "Animal");

            auto& mid_id = check<Wasp::Identifier>(nested_infix.right);
            EXPECT_EQ(mid_id.name, "Dog");
        }
    }
}

TEST(ParseExpressions, MemberAccessWithString) {
    auto mod = parse(R"(Animal.'Dog'.GermanShepherd)");
    ASSERT_EQ(mod.statements.size(), 1);

    auto& expr_stmt = check<Wasp::ExpressionStatement>(mod.statements[0]);

    // Top Level: [Left] . GermanShepherd
    {
        auto& top_infix = check<Wasp::Infix>(expr_stmt.expression);
        EXPECT_EQ(top_infix.op.type, Wasp::TokenType::DOT);

        auto& right_id = check<Wasp::Identifier>(top_infix.right);
        EXPECT_EQ(right_id.name, "GermanShepherd");

        // Nested Level: Animal . 'Dog'
        {
            auto& nested_infix = check<Wasp::Infix>(top_infix.left);
            EXPECT_EQ(nested_infix.op.type, Wasp::TokenType::DOT);

            auto& left_id = check<Wasp::Identifier>(nested_infix.left);
            EXPECT_EQ(left_id.name, "Animal");

            // FIX: Assert on the string node rather than just EXPECT_TRUE(true)
            auto& mid_str = check<std::string>(nested_infix.right); 
            EXPECT_EQ(mid_str, "Dog"); 
        }
    }
}


TEST(ParseExpressions, FunctionCallWithoutArguments) {
    auto mod = parse("get_worker()");
    ASSERT_EQ(mod.statements.size(), 1);

    auto& stmt = check<Wasp::ExpressionStatement>(mod.statements[0]);
    auto& call = check<Wasp::Call>(stmt.expression);
    
    auto& id = check<Wasp::Identifier>(call.callee);
    EXPECT_EQ(id.name, "get_worker");
    EXPECT_EQ(call.arguments.size(), 0);
}

TEST(ParseExpressions, FunctionCallWithMultipleArguments) {
    auto mod = parse("get_worker(1, 'John', true)");
    ASSERT_EQ(mod.statements.size(), 1);

    auto& stmt = check<Wasp::ExpressionStatement>(mod.statements[0]);
    ASSERT_NE(stmt.expression, nullptr);

    auto& call = check<Wasp::Call>(stmt.expression);
    ASSERT_EQ(call.arguments.size(), 3);
}

TEST(ParseExpressions, MethodCallWithArguments) {
    auto mod = parse("company.get_worker(1, 'John', true)");
    ASSERT_EQ(mod.statements.size(), 1);

    auto& stmt = check<Wasp::ExpressionStatement>(mod.statements[0]);
    
    auto& call = check<Wasp::Call>(stmt.expression);
    EXPECT_EQ(call.arguments.size(), 3);

    auto& callee_infix = check<Wasp::Infix>(call.callee);
    EXPECT_EQ(callee_infix.op.type, Wasp::TokenType::DOT);
    
    auto& left_id = check<Wasp::Identifier>(callee_infix.left);
    EXPECT_EQ(left_id.name, "company");
    
    auto& right_id = check<Wasp::Identifier>(callee_infix.right);
    EXPECT_EQ(right_id.name, "get_worker");
}

TEST(ParseExpressions, MethodCallWithArgumentsThenMemberAccess) {
    auto mod = parse("company.get_worker(1, 'John', true).name");
    ASSERT_EQ(mod.statements.size(), 1);

    auto& stmt = check<Wasp::ExpressionStatement>(mod.statements[0]);
    
    // Top Level: [company.get_worker(1, 'John', true)] . [name]
    auto& root_infix = check<Wasp::Infix>(stmt.expression);
    EXPECT_EQ(root_infix.op.type, Wasp::TokenType::DOT);

    {
        // Right side: 'name'
        auto& root_right_id = check<Wasp::Identifier>(root_infix.right);
        EXPECT_EQ(root_right_id.name, "name");
    }

    {
        // Left side: the call 'company.get_worker(1, 'John', true)'
        auto& call = check<Wasp::Call>(root_infix.left);
        EXPECT_EQ(call.arguments.size(), 3);

        // Check the callee of the call: 'company.get_worker'
        auto& callee_infix = check<Wasp::Infix>(call.callee);
        EXPECT_EQ(callee_infix.op.type, Wasp::TokenType::DOT);
        
        auto& callee_left_id = check<Wasp::Identifier>(callee_infix.left);
        EXPECT_EQ(callee_left_id.name, "company");
        
        auto& callee_right_id = check<Wasp::Identifier>(callee_infix.right);
        EXPECT_EQ(callee_right_id.name, "get_worker");
    }
}


TEST(ParseExpressions, PipeOutput) {
    auto mod = parse("foo() ~ bar(., 35) ~ boom(...)");
    ASSERT_EQ(mod.statements.size(), 1);

    auto& stmt = check<Wasp::ExpressionStatement>(mod.statements[0]);

    // Top Level: [foo() ~ bar(., 35)] ~ [boom(...)]
    auto& root_infix = check<Wasp::Infix>(stmt.expression);

    {
        // Right side: boom(...)
        auto& right_call = check<Wasp::Call>(root_infix.right);
        EXPECT_EQ(right_call.arguments.size(), 1);

        auto& boom_arg = check<Wasp::DotDotDotLiteral>(right_call.arguments[0]);
    }
    
    {
        // Left side: foo() ~ bar(., 35)
        auto& left_infix = check<Wasp::Infix>(root_infix.left);

        {
            // LeftInfix Right side: bar(., 35)
            auto& bar_call = check<Wasp::Call>(left_infix.right);
            ASSERT_EQ(bar_call.arguments.size(), 2);
            
            // Check '.' and '35' arguments
            auto& bar_arg1 = check<Wasp::DotLiteral>(bar_call.arguments[0]);
            auto& bar_arg2 = check<int>(bar_call.arguments[1]);
            EXPECT_EQ(bar_arg2, 35);
        }

        {
            // LeftInfix Left side: foo()
            auto& foo_call = check<Wasp::Call>(left_infix.left);
            EXPECT_EQ(foo_call.arguments.size(), 0);
        }
    }
}

