#include "CompilerTestBase.h"
#include "OpCode.h"

#include <cstddef>
#include <gtest/gtest.h>
#include <vector>

class CompileLoops : public CompilerTestBase
{
};

TEST_F(CompileLoops, ForLoop)
{
    auto actual_bytes = compile(R"(
for x in [1, 2, 3] do
    x
)");

    int val_1 = pool_size++;
    int val_2 = pool_size++;
    int val_3 = pool_size++;

    // clang-format off
    std::vector<std::byte> expected_bytes = {
        B(Wasp::OpCode::ENTER_MODULE),

        B(Wasp::OpCode::PUSH_SCOPE),
        B(Wasp::OpCode::LOAD_CONST),    B(val_1),
        B(Wasp::OpCode::LOAD_CONST),    B(val_2),
        B(Wasp::OpCode::LOAD_CONST),    B(val_3),
        B(Wasp::OpCode::BUILD_LIST),    B(3),
        B(Wasp::OpCode::GET_ITER),

        B(Wasp::OpCode::JUMP),          B(14), B(0),

        B(Wasp::OpCode::LOOP_ITER),     B(27), B(0),
        B(Wasp::OpCode::JUMP),          B(20), B(0),

        B(Wasp::OpCode::GET_LOCAL),     B(1),
        B(Wasp::OpCode::POP),
        B(Wasp::OpCode::POP),
        B(Wasp::OpCode::JUMP),          B(14), B(0),

        B(Wasp::OpCode::POP),
        B(Wasp::OpCode::POP_SCOPE),
        B(Wasp::OpCode::JUMP),          B(32), B(0),


        B(Wasp::OpCode::EXIT_MODULE),   B(0)
    };
    // clang-format on

    EXPECT_EQ(actual_bytes, expected_bytes);
}

TEST_F(CompileLoops, WhileLoop)
{
    auto actual_bytes = compile(R"(
while true do
    25
)");

    int val_25 = pool_size++;

    // clang-format off
  std::vector<std::byte> expected_bytes = {
      B(Wasp::OpCode::ENTER_MODULE),
      B(Wasp::OpCode::JUMP),          B(4), B(0),

      // --- Header ---
      B(Wasp::OpCode::PUSH_SCOPE),
      B(Wasp::OpCode::LOAD_TRUE),
      B(Wasp::OpCode::JUMP_IF_FALSE), B(21), B(0), // Jump to Trampoline
      B(Wasp::OpCode::JUMP),          B(12), B(0),

      // --- Body ---
      B(Wasp::OpCode::PUSH_SCOPE),
      B(Wasp::OpCode::LOAD_CONST),    B(val_25),
      B(Wasp::OpCode::POP),                        // Expression cleanup
      B(Wasp::OpCode::POP_SCOPE),                  // Pop Body Scope
      B(Wasp::OpCode::POP_SCOPE),                  // Pop Condition Scope
      B(Wasp::OpCode::JUMP),          B(4), B(0),  // Loop back to Header

      // --- Exit Trampoline ---
      B(Wasp::OpCode::POP_SCOPE),
      B(Wasp::OpCode::JUMP),          B(25), B(0),

      // --- End ---
      B(Wasp::OpCode::JUMP),          B(28), B(0),
      B(Wasp::OpCode::EXIT_MODULE), B(0)
  };
    // clang-format on

    EXPECT_EQ(actual_bytes, expected_bytes);
}

TEST_F(CompileLoops, WhileBreak)
{
    auto actual_bytes = compile(R"(
while true do
    25 + 25
    break
)");

    int val_25 = pool_size++;

    // clang-format off
  std::vector<std::byte> expected_bytes = {
      B(Wasp::OpCode::ENTER_MODULE),
      B(Wasp::OpCode::JUMP),          B(4), B(0),

      // --- Header ---
      B(Wasp::OpCode::PUSH_SCOPE),
      B(Wasp::OpCode::LOAD_TRUE),
      B(Wasp::OpCode::JUMP_IF_FALSE), B(29), B(0),
      B(Wasp::OpCode::JUMP),          B(12), B(0),

      // --- Body ---
      B(Wasp::OpCode::PUSH_SCOPE),

      // Expression: 25 + 25
      B(Wasp::OpCode::LOAD_CONST),    B(val_25),
      B(Wasp::OpCode::LOAD_CONST),    B(val_25),
      B(Wasp::OpCode::ADD),
      B(Wasp::OpCode::POP),

      // Break Statement
      B(Wasp::OpCode::POP_SCOPE),
      B(Wasp::OpCode::POP_SCOPE),
      B(Wasp::OpCode::JUMP),          B(33), B(0),

      // Normal Continuation (Dead code due to break)
      B(Wasp::OpCode::POP_SCOPE),
      B(Wasp::OpCode::POP_SCOPE),
      B(Wasp::OpCode::JUMP),          B(4), B(0),

      // --- Exit Trampoline ---
      B(Wasp::OpCode::POP_SCOPE),
      B(Wasp::OpCode::JUMP),          B(33), B(0),

      // --- End ---
      B(Wasp::OpCode::JUMP),          B(36), B(0),
      B(Wasp::OpCode::EXIT_MODULE), B(0)
  };
    // clang-format on

    EXPECT_EQ(actual_bytes, expected_bytes);
}

TEST_F(CompileLoops, WhileRedo)
{
    auto actual_bytes = compile(R"(
while true do
    25 + 25
    redo
)");

    int val_25 = pool_size++;

    // clang-format off
  std::vector<std::byte> expected_bytes = {
      B(Wasp::OpCode::ENTER_MODULE),
      B(Wasp::OpCode::JUMP),          B(4), B(0),

      // --- Header ---
      B(Wasp::OpCode::PUSH_SCOPE),
      B(Wasp::OpCode::LOAD_TRUE),
      B(Wasp::OpCode::JUMP_IF_FALSE), B(28), B(0),
      B(Wasp::OpCode::JUMP),          B(12), B(0),

      // --- Body ---
      B(Wasp::OpCode::PUSH_SCOPE),

      // Expression: 25 + 25
      B(Wasp::OpCode::LOAD_CONST),    B(val_25),
      B(Wasp::OpCode::LOAD_CONST),    B(val_25),
      B(Wasp::OpCode::ADD),
      B(Wasp::OpCode::POP),

      // Redo Statement
      B(Wasp::OpCode::POP_SCOPE),                  // Pop ONLY the Body Scope
      B(Wasp::OpCode::JUMP),          B(12), B(0), // Jump directly back to Body Scope creation

      // Normal Continuation (Dead Code due to Redo)
      B(Wasp::OpCode::POP_SCOPE),
      B(Wasp::OpCode::POP_SCOPE),
      B(Wasp::OpCode::JUMP),          B(4), B(0),

      // --- Exit Trampoline ---
      B(Wasp::OpCode::POP_SCOPE),
      B(Wasp::OpCode::JUMP),          B(32), B(0),

      // --- End ---
      B(Wasp::OpCode::JUMP),          B(35), B(0),
      B(Wasp::OpCode::EXIT_MODULE), B(0)
  };
    // clang-format on

    EXPECT_EQ(actual_bytes, expected_bytes);
}

TEST_F(CompileLoops, WhileWithIfAndAssignment)
{
    auto actual_bytes = compile(R"(
let x = 1

while x < 5 do
    if x % 2 == 0 then
        print(x)

    x = x + 1
)");

    int val_1 = pool_size++;
    int val_5 = pool_size++;
    int val_2 = pool_size++;
    int val_0 = pool_size++;

    // clang-format off
    std::vector<std::byte> expected_bytes = {
        B(Wasp::OpCode::ENTER_MODULE),

        B(Wasp::OpCode::LOAD_CONST),    B(val_1),
        B(Wasp::OpCode::SET_LOCAL),     B(0),
        B(Wasp::OpCode::JUMP),          B(8), B(0),

        B(Wasp::OpCode::PUSH_SCOPE),
        B(Wasp::OpCode::GET_LOCAL),     B(0),
        B(Wasp::OpCode::LOAD_CONST),    B(val_5),
        B(Wasp::OpCode::LT),
        B(Wasp::OpCode::JUMP_IF_FALSE), B(24), B(0),
        B(Wasp::OpCode::JUMP),          B(20), B(0),

        B(Wasp::OpCode::PUSH_SCOPE),
        B(Wasp::OpCode::JUMP),          B(31), B(0),

        B(Wasp::OpCode::POP_SCOPE),
        B(Wasp::OpCode::JUMP),          B(28), B(0),
        B(Wasp::OpCode::JUMP),          B(75), B(0),

        B(Wasp::OpCode::PUSH_SCOPE),
        B(Wasp::OpCode::GET_LOCAL),     B(0),
        B(Wasp::OpCode::LOAD_CONST),    B(val_2),
        B(Wasp::OpCode::MOD),
        B(Wasp::OpCode::LOAD_CONST),    B(val_0),
        B(Wasp::OpCode::EQ),
        B(Wasp::OpCode::JUMP_IF_FALSE), B(57), B(0),
        B(Wasp::OpCode::JUMP),          B(46), B(0),

        B(Wasp::OpCode::GET_NATIVE),    B(0),
        B(Wasp::OpCode::GET_LOCAL),     B(0),
        B(Wasp::OpCode::CALL),          B(1),
        B(Wasp::OpCode::POP),
        B(Wasp::OpCode::POP_SCOPE),
        B(Wasp::OpCode::JUMP),          B(62), B(0),

        B(Wasp::OpCode::POP_SCOPE),
        B(Wasp::OpCode::PUSH_SCOPE),
        B(Wasp::OpCode::JUMP),          B(62), B(0),

        B(Wasp::OpCode::GET_LOCAL),     B(0),
        B(Wasp::OpCode::LOAD_CONST),    B(val_1),
        B(Wasp::OpCode::ADD),
        B(Wasp::OpCode::SET_LOCAL),     B(0),
        B(Wasp::OpCode::POP),

        B(Wasp::OpCode::POP_SCOPE),
        B(Wasp::OpCode::POP_SCOPE),
        B(Wasp::OpCode::JUMP),          B(8), B(0),

        B(Wasp::OpCode::GET_LOCAL),     B(0),
        B(Wasp::OpCode::EXIT_MODULE),   B(1),
    };
    // clang-format on

    EXPECT_EQ(actual_bytes, expected_bytes);
}
