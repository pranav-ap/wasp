#include "CompilerTestBase.h"
#include "OpCode.h"

#include <cstddef>
#include <gtest/gtest.h>
#include <vector>

class CompileVariables : public CompilerTestBase
{
};

TEST_F(CompileVariables, DefineAndUseVariable)
{
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

        // let x = 42
        B(Wasp::OpCode::LOAD_CONST),    B(val_42), // 42 stays on stack as Slot 0

        // x + 1
        B(Wasp::OpCode::GET_LOCAL),     B(var_x),  // "x"
        B(Wasp::OpCode::LOAD_CONST),    B(val_1),  // 1
        B(Wasp::OpCode::ADD),
        B(Wasp::OpCode::POP),

        // End of Block
        B(Wasp::OpCode::JUMP),          B(12), B(0),

        // Export Builder: Push `x` (Slot 0) to be bundled
        B(Wasp::OpCode::GET_LOCAL),     B(var_x),
        B(Wasp::OpCode::EXIT_MODULE),   B(1)       // Bundle 1 export
    };
    // clang-format on

    EXPECT_EQ(actual_bytes, expected_bytes);
}

TEST_F(CompileVariables, DefineAndReAssignVariable)
{
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

        // let x = 42
        B(Wasp::OpCode::LOAD_CONST),    B(val_42), // Slot 0

        // x = x + 1
        B(Wasp::OpCode::GET_LOCAL),     B(var_x),  // "x"
        B(Wasp::OpCode::LOAD_CONST),    B(val_1),  // 1
        B(Wasp::OpCode::ADD),
        B(Wasp::OpCode::SET_LOCAL),     B(var_x),  // Overwrite Slot 0
        B(Wasp::OpCode::POP),                      // Pop assignment result

        // End of Block
        B(Wasp::OpCode::JUMP),          B(14), B(0),

        // Export Builder: Push `x` (Slot 0) to be bundled
        B(Wasp::OpCode::GET_LOCAL),     B(var_x),
        B(Wasp::OpCode::EXIT_MODULE),   B(1)       // Bundle 1 export
    };
    // clang-format on

    EXPECT_EQ(actual_bytes, expected_bytes);
}
