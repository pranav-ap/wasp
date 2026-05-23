#include "CompilerTestBase.h"
#include "OpCode.h"

#include <cstddef>
#include <gtest/gtest.h>
#include <vector>

class CompileExpressions : public CompilerTestBase {};

// ============================================================================
// Basic Primitives
// ============================================================================

TEST_F(CompileExpressions, SimpleInteger) {
    auto actual_bytes = compile("25");
    int val_25 = pool_size++;

    // clang-format off
  std::vector<std::byte> expected_bytes = {
      B(Wasp::OpCode::ENTER_MODULE),
      B(Wasp::OpCode::LOAD_CONSTANT),   B(val_25),
      B(Wasp::OpCode::POP),
      B(Wasp::OpCode::JUMP),         B(7), B(0),
      B(Wasp::OpCode::EXIT_MODULE), B(0)
  };
    // clang-format on

    EXPECT_EQ(actual_bytes, expected_bytes);
}

TEST_F(CompileExpressions, SimpleList) {
    auto actual_bytes = compile("[1, 2, 3]");
    int val_1 = pool_size++;
    int val_2 = pool_size++;
    int val_3 = pool_size++;

    // clang-format off
  std::vector<std::byte> expected_bytes = {
      B(Wasp::OpCode::ENTER_MODULE),
      B(Wasp::OpCode::LOAD_CONSTANT),   B(val_1),
      B(Wasp::OpCode::LOAD_CONSTANT),   B(val_2),
      B(Wasp::OpCode::LOAD_CONSTANT),   B(val_3),
      B(Wasp::OpCode::BUILD_LIST),   B(3),
      B(Wasp::OpCode::POP),
      B(Wasp::OpCode::JUMP),         B(13), B(0),
      B(Wasp::OpCode::EXIT_MODULE), B(0)
  };
    // clang-format on

    EXPECT_EQ(actual_bytes, expected_bytes);
}

// ============================================================================
// Arithmetic
// ============================================================================

TEST_F(CompileExpressions, NegateNumber) {
    auto actual_bytes = compile("-2");
    int val_2 = pool_size++;

    // clang-format off
  std::vector<std::byte> expected_bytes = {
      B(Wasp::OpCode::ENTER_MODULE),
      B(Wasp::OpCode::LOAD_CONSTANT),   B(val_2),
      B(Wasp::OpCode::NEGATE),
      B(Wasp::OpCode::POP),
      B(Wasp::OpCode::JUMP),         B(8), B(0),
      B(Wasp::OpCode::EXIT_MODULE), B(0)
  };
    // clang-format on

    EXPECT_EQ(actual_bytes, expected_bytes);
}

TEST_F(CompileExpressions, SimpleAddition) {
    auto actual_bytes = compile("1 + 2");
    int val_1 = pool_size++;
    int val_2 = pool_size++;

    // clang-format off
  std::vector<std::byte> expected_bytes = {
      B(Wasp::OpCode::ENTER_MODULE),
      B(Wasp::OpCode::LOAD_CONSTANT),   B(val_1),
      B(Wasp::OpCode::LOAD_CONSTANT),   B(val_2),
      B(Wasp::OpCode::ADD),
      B(Wasp::OpCode::POP),
      B(Wasp::OpCode::JUMP),         B(10), B(0),
      B(Wasp::OpCode::EXIT_MODULE), B(0)
  };
    // clang-format on

    EXPECT_EQ(actual_bytes, expected_bytes);
}
