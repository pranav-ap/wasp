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

        B(Wasp::OpCode::PUSH_SCOPE),
        B(Wasp::OpCode::LOAD_TRUE),
        B(Wasp::OpCode::JUMP_IF_FALSE), B(14), B(0),

        B(Wasp::OpCode::PUSH_SCOPE),
        B(Wasp::OpCode::LOAD_CONST),    B(val_3),
        B(Wasp::OpCode::POP_SCOPE_KEEP_TOS),
        B(Wasp::OpCode::POP_SCOPE_KEEP_TOS),
        B(Wasp::OpCode::JUMP),          B(23), B(0),

        B(Wasp::OpCode::POP_SCOPE),
        B(Wasp::OpCode::POP_SCOPE),
        B(Wasp::OpCode::PUSH_SCOPE),
        B(Wasp::OpCode::LOAD_CONST),    B(val_1),
        B(Wasp::OpCode::POP_SCOPE_KEEP_TOS),
        B(Wasp::OpCode::JUMP),          B(23), B(0),

        B(Wasp::OpCode::POP),
        B(Wasp::OpCode::JUMP),          B(27), B(0),

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

        B(Wasp::OpCode::PUSH_SCOPE),
        B(Wasp::OpCode::LOAD_TRUE),
        B(Wasp::OpCode::JUMP_IF_FALSE), B(14), B(0),

        B(Wasp::OpCode::PUSH_SCOPE),
        B(Wasp::OpCode::LOAD_CONST),    B(val_3),
        B(Wasp::OpCode::POP_SCOPE_KEEP_TOS),
        B(Wasp::OpCode::POP_SCOPE_KEEP_TOS),
        B(Wasp::OpCode::JUMP),          B(23), B(0),

        B(Wasp::OpCode::POP_SCOPE),
        B(Wasp::OpCode::POP_SCOPE),
        B(Wasp::OpCode::PUSH_SCOPE),
        B(Wasp::OpCode::LOAD_CONST),    B(val_1),
        B(Wasp::OpCode::POP_SCOPE_KEEP_TOS),
        B(Wasp::OpCode::JUMP),          B(23), B(0),

        B(Wasp::OpCode::POP),
        B(Wasp::OpCode::JUMP),          B(27), B(0),

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

        // --- Outer If ---
        B(Wasp::OpCode::PUSH_SCOPE),
        B(Wasp::OpCode::LOAD_FALSE),
        B(Wasp::OpCode::JUMP_IF_FALSE), B(17), B(0),
        B(Wasp::OpCode::JUMP),          B(9),  B(0),

        // Outer True
        B(Wasp::OpCode::PUSH_SCOPE),
        B(Wasp::OpCode::LOAD_CONST),    B(val_25),
        B(Wasp::OpCode::POP),
        B(Wasp::OpCode::POP_SCOPE),
        B(Wasp::OpCode::JUMP),          B(25), B(0), // Jump to Outer End

        // --- Outer False (Elif Condition) ---
        B(Wasp::OpCode::PUSH_SCOPE),                 // Elif Condition Scope
        B(Wasp::OpCode::LOAD_TRUE),
        B(Wasp::OpCode::JUMP_IF_FALSE), B(37), B(0),
        B(Wasp::OpCode::JUMP),          B(29), B(0),

        // --- Outer Converge (Safely pops outer condition) ---
        B(Wasp::OpCode::POP_SCOPE),
        B(Wasp::OpCode::JUMP),          B(49), B(0),

        // --- Elif True ---
        B(Wasp::OpCode::PUSH_SCOPE),
        B(Wasp::OpCode::LOAD_CONST),    B(val_25),
        B(Wasp::OpCode::POP),
        B(Wasp::OpCode::POP_SCOPE),
        B(Wasp::OpCode::JUMP),          B(45), B(0), // Jump to Inner End

        // --- Elif False (Else Body) ---
        B(Wasp::OpCode::PUSH_SCOPE),
        B(Wasp::OpCode::LOAD_CONST),    B(val_25),
        B(Wasp::OpCode::POP),
        B(Wasp::OpCode::POP_SCOPE),
        B(Wasp::OpCode::JUMP),          B(45), B(0), // Jump to Inner End

        // --- Inner Converge (Safely pops inner condition) ---
        B(Wasp::OpCode::POP_SCOPE),
        B(Wasp::OpCode::JUMP),          B(25), B(0), // Jump back up to Outer Converge

        // --- Final Module Exit ---
        B(Wasp::OpCode::EXIT_MODULE),   B(0)
    };
    // clang-format on

    EXPECT_EQ(actual_bytes, expected_bytes);
}
