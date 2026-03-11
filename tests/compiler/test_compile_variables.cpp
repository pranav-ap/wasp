#include "CompilerTestBase.h"
#include "OpCode.h"

#include <cstddef>
#include <gtest/gtest.h>
#include <vector>

class CompileVariables : public CompilerTestBase {};

TEST_F(CompileVariables, DefineAndUseVariable) {
    auto actual_bytes = compile(R"(
let x = 42
x + 1
)");

    int val_42 = pool_size++;
    int val_1 = pool_size++;

    int var_x = 0;

    // clang-format off
  std::vector<std::byte> expected_bytes = {
      B(Wasp::OpCode::ENTER_MODULE),
      B(Wasp::OpCode::LOAD_CONST),    B(val_42), // 42
      B(Wasp::OpCode::DEFINE_LOCAL),  B(var_x),

      B(Wasp::OpCode::GET_LOCAL),     B(var_x),  // "x"
      B(Wasp::OpCode::LOAD_CONST),    B(val_1),  // 1
      B(Wasp::OpCode::ADD),
      B(Wasp::OpCode::POP),
      B(Wasp::OpCode::JUMP),          B(14), B(0),
      B(Wasp::OpCode::EXIT_MODULE)
  };
    // clang-format on

    EXPECT_EQ(actual_bytes, expected_bytes);
}

TEST_F(CompileVariables, DefineAndReAssignVariable) {
    auto actual_bytes = compile(R"(
let x = 42
x = x + 1
)");

    int val_42 = pool_size++;
    int val_1 = pool_size++;

    int var_x = 0;

    // clang-format off
  std::vector<std::byte> expected_bytes = {
      B(Wasp::OpCode::ENTER_MODULE),
      B(Wasp::OpCode::LOAD_CONST),    B(val_42),
      B(Wasp::OpCode::DEFINE_LOCAL),  B(var_x),
      
      B(Wasp::OpCode::GET_LOCAL),     B(var_x),
      B(Wasp::OpCode::LOAD_CONST),    B(val_1),
      B(Wasp::OpCode::ADD),
      B(Wasp::OpCode::SET_LOCAL),     B(var_x),
      B(Wasp::OpCode::POP),
      
      B(Wasp::OpCode::JUMP),          B(16), B(0),
      B(Wasp::OpCode::EXIT_MODULE)
  };
    // clang-format on

    EXPECT_EQ(actual_bytes, expected_bytes);
}