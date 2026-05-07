#include "CompilerTestBase.h"
#include "OpCode.h"

#include <cstddef>
#include <gtest/gtest.h>
#include <vector>

class CompileBranches : public CompilerTestBase
{
};

TEST_F(CompileBranches, IfTernary)
{
    auto actual_bytes = compile(R"(
if true then 3 else 1
)");

    int val_3 = pool_size++;
    int val_1 = pool_size++;

    // clang-format off
    std::vector<std::byte> expected_bytes = {
        B(Wasp::OpCode::ENTER_MODULE),

        B(Wasp::OpCode::JUMP),          B(4), B(0),
        B(Wasp::OpCode::PUSH_SCOPE),
        B(Wasp::OpCode::LOAD_TRUE),
        B(Wasp::OpCode::JUMP_IF_FALSE), B(20), B(0),
        B(Wasp::OpCode::JUMP),          B(12), B(0),

        B(Wasp::OpCode::PUSH_SCOPE),
        B(Wasp::OpCode::LOAD_CONST),    B(val_3),
        B(Wasp::OpCode::POP_SCOPE_KEEP_TOS),
        B(Wasp::OpCode::POP_SCOPE_KEEP_TOS),
        B(Wasp::OpCode::JUMP),          B(28), B(0),

        B(Wasp::OpCode::POP_SCOPE),
        B(Wasp::OpCode::PUSH_SCOPE),
        B(Wasp::OpCode::LOAD_CONST),    B(val_1),
        B(Wasp::OpCode::POP_SCOPE_KEEP_TOS),
        B(Wasp::OpCode::JUMP),          B(28), B(0),

        B(Wasp::OpCode::POP),
        B(Wasp::OpCode::JUMP),          B(32), B(0),

        B(Wasp::OpCode::EXIT_MODULE),   B(0)
    };
    // clang-format on

    EXPECT_EQ(actual_bytes, expected_bytes);
}

TEST_F(CompileBranches, IfLet)
{
    auto actual_bytes = compile(R"(
if let x = true then 3 else 1
)");

    int val_3 = pool_size++;
    int val_1 = pool_size++;

    // clang-format off
    std::vector<std::byte> expected_bytes = {
        B(Wasp::OpCode::ENTER_MODULE),

        B(Wasp::OpCode::JUMP),          B(4), B(0),
        B(Wasp::OpCode::PUSH_SCOPE),
        B(Wasp::OpCode::LOAD_TRUE),
        B(Wasp::OpCode::SET_LOCAL),     B(0),
        B(Wasp::OpCode::GET_LOCAL),     B(0),
        B(Wasp::OpCode::JUMP_IF_FALSE), B(24), B(0),
        B(Wasp::OpCode::JUMP),          B(16), B(0),

        B(Wasp::OpCode::PUSH_SCOPE),
        B(Wasp::OpCode::LOAD_CONST),    B(val_3),
        B(Wasp::OpCode::POP_SCOPE_KEEP_TOS),
        B(Wasp::OpCode::POP_SCOPE_KEEP_TOS),
        B(Wasp::OpCode::JUMP),          B(32), B(0),

        B(Wasp::OpCode::POP_SCOPE),
        B(Wasp::OpCode::PUSH_SCOPE),
        B(Wasp::OpCode::LOAD_CONST),    B(val_1),
        B(Wasp::OpCode::POP_SCOPE_KEEP_TOS),
        B(Wasp::OpCode::JUMP),          B(32), B(0),

        B(Wasp::OpCode::POP),
        B(Wasp::OpCode::JUMP),          B(36), B(0),

        B(Wasp::OpCode::EXIT_MODULE),   B(0)
    };
    // clang-format on

    EXPECT_EQ(actual_bytes, expected_bytes);
}

TEST_F(CompileBranches, IfElIfElse)
{
    auto actual_bytes = compile(R"(
if false then
    25
elif true then
    25
else
    25
)");

    int val_25 = pool_size++;

    // clang-format off
    std::vector<std::byte> expected_bytes = {
        B(Wasp::OpCode::ENTER_MODULE),

        B(Wasp::OpCode::JUMP),          B(4), B(0),

        B(Wasp::OpCode::PUSH_SCOPE),
        B(Wasp::OpCode::LOAD_FALSE),
        B(Wasp::OpCode::JUMP_IF_FALSE), B(19), B(0),
        B(Wasp::OpCode::JUMP),          B(12), B(0),

        B(Wasp::OpCode::LOAD_CONST),    B(val_25),
        B(Wasp::OpCode::POP),
        B(Wasp::OpCode::POP_SCOPE),
        B(Wasp::OpCode::JUMP),          B(23), B(0),

        B(Wasp::OpCode::POP_SCOPE),
        B(Wasp::OpCode::JUMP),          B(26), B(0),

        B(Wasp::OpCode::JUMP),          B(53), B(0),

        B(Wasp::OpCode::PUSH_SCOPE),
        B(Wasp::OpCode::LOAD_TRUE),
        B(Wasp::OpCode::JUMP_IF_FALSE), B(41), B(0),
        B(Wasp::OpCode::JUMP),          B(34), B(0),

        B(Wasp::OpCode::LOAD_CONST),    B(val_25),
        B(Wasp::OpCode::POP),
        B(Wasp::OpCode::POP_SCOPE),
        B(Wasp::OpCode::JUMP),          B(50), B(0),

        B(Wasp::OpCode::POP_SCOPE),
        B(Wasp::OpCode::PUSH_SCOPE),
        B(Wasp::OpCode::LOAD_CONST),    B(val_25),
        B(Wasp::OpCode::POP),
        B(Wasp::OpCode::POP_SCOPE),
        B(Wasp::OpCode::JUMP),          B(50), B(0),

        B(Wasp::OpCode::JUMP),          B(23), B(0),

        B(Wasp::OpCode::EXIT_MODULE),   B(0)
    };
    // clang-format on

    EXPECT_EQ(actual_bytes, expected_bytes);
}
