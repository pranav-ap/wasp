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
        B(Wasp::OpCode::ENTER_MODULE),               // 0000

        B(Wasp::OpCode::JUMP),          B(4), B(0),  // 0001 | Explicit entry into Outer If

        // --- Outer Test Block (0004) ---
        B(Wasp::OpCode::PUSH_SCOPE),                 // 0004 | test and true branch scope
        B(Wasp::OpCode::LOAD_FALSE),                 // 0005
        B(Wasp::OpCode::JUMP_IF_FALSE), B(19), B(0), // 0006 | Jump to Outer False Block
        B(Wasp::OpCode::JUMP),          B(12), B(0), // 0009 | Jump to Outer True Block

        // --- Outer True Block (0012) ---
        B(Wasp::OpCode::LOAD_CONST),    B(val_25),   // 0012
        B(Wasp::OpCode::POP),                        // 0014
        B(Wasp::OpCode::POP_SCOPE),                  // 0015 | Clean up test/true scope
        B(Wasp::OpCode::JUMP),          B(24), B(0), // 0016 | Jump to Outer End Block

        // --- Outer False Block (0019) ---
        B(Wasp::OpCode::POP_SCOPE),                  // 0019 | Clean up test/true scope
        B(Wasp::OpCode::PUSH_SCOPE),                 // 0020 | false branch scope
        B(Wasp::OpCode::JUMP),          B(27), B(0), // 0021 | Jump into Elif statement

        // --- Outer End Block (0024) ---
        B(Wasp::OpCode::JUMP),          B(55), B(0), // 0024 | Jump to Module Exit

        // --- Inner (Elif) Test Block (0027) ---
        B(Wasp::OpCode::PUSH_SCOPE),                 // 0027 | test and true branch scope
        B(Wasp::OpCode::LOAD_TRUE),                  // 0028
        B(Wasp::OpCode::JUMP_IF_FALSE), B(42), B(0), // 0029 | Jump to Inner False Block
        B(Wasp::OpCode::JUMP),          B(35), B(0), // 0032 | Jump to Inner True Block

        // --- Inner (Elif) True Block (0035) ---
        B(Wasp::OpCode::LOAD_CONST),    B(val_25),   // 0035
        B(Wasp::OpCode::POP),                        // 0037
        B(Wasp::OpCode::POP_SCOPE),                  // 0038 | Clean up test/true scope
        B(Wasp::OpCode::JUMP),          B(52), B(0), // 0039 | Jump to Inner End Block

        // --- Inner (Else) False Block (0042) ---
        B(Wasp::OpCode::POP_SCOPE),                  // 0042 | Clean up test/true scope
        B(Wasp::OpCode::PUSH_SCOPE),                 // 0043 | false branch scope
        B(Wasp::OpCode::PUSH_SCOPE),                 // 0044 | else branch scope
        B(Wasp::OpCode::LOAD_CONST),    B(val_25),   // 0045
        B(Wasp::OpCode::POP),                        // 0047
        B(Wasp::OpCode::POP_SCOPE),                  // 0048 | Clean up else branch scope
        B(Wasp::OpCode::JUMP),          B(52), B(0), // 0049 | Jump to Inner End Block

        // --- Inner End Block (0052) ---
        B(Wasp::OpCode::JUMP),          B(24), B(0), // 0052 | Jump back to Outer End Block

        // --- Exit Block (0055) ---
        B(Wasp::OpCode::EXIT_MODULE),   B(0)         // 0055
    };
    // clang-format on

    EXPECT_EQ(actual_bytes, expected_bytes);
}
