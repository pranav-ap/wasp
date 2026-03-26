#include "CompilerTestBase.h"
#include "OpCode.h"

#include <cstddef>
#include <gtest/gtest.h>
#include <vector>

class CompileBranches : public CompilerTestBase {};

// ============================================================================
// Control Flow: Branches
// ============================================================================

TEST_F(CompileBranches, IfTernary) {
    auto actual_bytes = compile(R"(
if false then 25 else 10
)");

    int val_25 = pool_size++;
    int val_10 = pool_size++;

    // clang-format off
  std::vector<std::byte> expected_bytes = {
      B(Wasp::OpCode::ENTER_MODULE),
      B(Wasp::OpCode::PUSH_SCOPE),                        // Scope 1 (Test Scope)
      B(Wasp::OpCode::LOAD_FALSE),
      B(Wasp::OpCode::JUMP_IF_FALSE), B(15), B(0),
      B(Wasp::OpCode::JUMP),          B(9),  B(0),

      // True Expression
      B(Wasp::OpCode::LOAD_CONST),    B(val_25),          // Value: 25
      B(Wasp::OpCode::POP_SCOPE),                         // Pop Scope 1
      B(Wasp::OpCode::JUMP),          B(23), B(0),        // Jump to Converge

      // False Expression
      B(Wasp::OpCode::POP_SCOPE),                         // Pop Scope 1 (Clean up Test Scope)
      B(Wasp::OpCode::PUSH_SCOPE),                        // Scope 2 (Else Branch Scope)
      B(Wasp::OpCode::LOAD_CONST),    B(val_10),          // Value: 10
      B(Wasp::OpCode::POP_SCOPE),                         // Pop Scope 2
      B(Wasp::OpCode::JUMP),          B(23), B(0),        // Jump to Converge

      // Converge (End of Statement)
      B(Wasp::OpCode::POP),                               // Pop the result
      B(Wasp::OpCode::JUMP),          B(27), B(0),
      B(Wasp::OpCode::EXIT_MODULE)
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
      B(Wasp::OpCode::PUSH_SCOPE),                        // Scope 1 (Outer Condition)
      B(Wasp::OpCode::LOAD_FALSE),
      B(Wasp::OpCode::JUMP_IF_FALSE), B(21), B(0),
      B(Wasp::OpCode::JUMP),          B(9),  B(0),

      // Outer True
      B(Wasp::OpCode::PUSH_SCOPE),                        // True Branch Sub-scope
      B(Wasp::OpCode::LOAD_CONST),    B(val_25),
      B(Wasp::OpCode::POP),
      B(Wasp::OpCode::POP_SCOPE),                         // Pop True Branch Sub-scope
      B(Wasp::OpCode::POP_SCOPE),                         // Pop Scope 1 (Condition)
      B(Wasp::OpCode::JUMP),          B(18), B(0),
      B(Wasp::OpCode::JUMP),          B(51), B(0),        // Jump to Exit

      // --- Outer False (Trampoline) ---
      B(Wasp::OpCode::POP_SCOPE),                         // Pop Scope 1 (Condition)

      // --- Elif (Inner If) ---
      B(Wasp::OpCode::PUSH_SCOPE),                        // Scope 2 (Elif Condition)
      B(Wasp::OpCode::LOAD_TRUE),
      B(Wasp::OpCode::JUMP_IF_FALSE), B(42), B(0),
      B(Wasp::OpCode::JUMP),          B(30), B(0),

      // Elif True
      B(Wasp::OpCode::PUSH_SCOPE),                        // Elif True Branch Sub-scope
      B(Wasp::OpCode::LOAD_CONST),    B(val_25),
      B(Wasp::OpCode::POP),
      B(Wasp::OpCode::POP_SCOPE),                         // Pop Elif True Branch Sub-scope
      B(Wasp::OpCode::POP_SCOPE),                         // Pop Scope 2 (Condition)
      B(Wasp::OpCode::JUMP),          B(39), B(0),
      B(Wasp::OpCode::JUMP),          B(18), B(0),        // Jump to Outer End (18)

      // --- Elif False (Trampoline) ---
      B(Wasp::OpCode::POP_SCOPE),                         // Pop Scope 2 (Condition)

      // --- Else ---
      B(Wasp::OpCode::PUSH_SCOPE),                        // Else Branch wrapper
      B(Wasp::OpCode::PUSH_SCOPE),                        // Else Body Sub-scope
      B(Wasp::OpCode::LOAD_CONST),    B(val_25),
      B(Wasp::OpCode::POP),
      B(Wasp::OpCode::POP_SCOPE),                         // Pop Else Body Sub-scope
      B(Wasp::OpCode::POP_SCOPE),                         // Pop Else Branch wrapper
      B(Wasp::OpCode::JUMP),          B(39), B(0),        // Jump to Elif End (39)

      B(Wasp::OpCode::EXIT_MODULE)
  };
    // clang-format on

    EXPECT_EQ(actual_bytes, expected_bytes);
}

TEST_F(CompileBranches, IfLet)
{
    auto actual_bytes = compile(R"(
if let x = true then
    25
)");

    int val_25 = pool_size++;

    // clang-format off
  std::vector<std::byte> expected_bytes = {
      B(Wasp::OpCode::ENTER_MODULE),

      // Condition Scope
      B(Wasp::OpCode::PUSH_SCOPE),
      B(Wasp::OpCode::LOAD_TRUE),
      B(Wasp::OpCode::DUP),

      B(Wasp::OpCode::JUMP_IF_FALSE), B(23), B(0),
      B(Wasp::OpCode::JUMP),          B(10), B(0),

      // True Block
      B(Wasp::OpCode::PUSH_SCOPE),                        // True Branch Sub-scope
      B(Wasp::OpCode::LOAD_CONST),    B(val_25),
      B(Wasp::OpCode::POP),
      B(Wasp::OpCode::POP),                               // Pop DUP'd condition
      B(Wasp::OpCode::POP_SCOPE),                         // Pop True Branch Sub-scope
      B(Wasp::OpCode::POP_SCOPE),                         // Pop Condition Scope
      B(Wasp::OpCode::JUMP),          B(20), B(0),
      B(Wasp::OpCode::JUMP),          B(27), B(0),

      // False Path (Trampoline)
      B(Wasp::OpCode::POP_SCOPE),                         // Pop Condition Scope
      B(Wasp::OpCode::JUMP),          B(20), B(0),

      B(Wasp::OpCode::EXIT_MODULE)
  };
    // clang-format on

    EXPECT_EQ(actual_bytes, expected_bytes);
}
