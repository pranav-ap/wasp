#include "CFGraph.h"
#include "CompilerTestBase.h"
#include "Objects.h"
#include "OpCode.h"

#include <cstddef>
#include <gtest/gtest.h>
#include <memory>
#include <vector>

class CompileFunctions : public CompilerTestBase
{
};

TEST_F(CompileFunctions, AddFunction)
{
    auto actual_bytes = compile(R"(
fun add(a: int, b: int) => int
    return a + b
)");

    int func_id = pool_size++;
    int var_add = 0;

    // clang-format off
    std::vector<std::byte> expected_bytes = {
        B(Wasp::OpCode::ENTER_MODULE),

        B(Wasp::OpCode::LOAD_CONST),        B(func_id),
        B(Wasp::OpCode::MAKE_FUNCTION),     B(0),
        B(Wasp::OpCode::STORE_FUNCTION_OVERLOAD), B(var_add),

        B(Wasp::OpCode::JUMP),              B(10), B(0),

        // Export Builder
        B(Wasp::OpCode::GET_LOCAL),         B(var_add),
        B(Wasp::OpCode::EXIT_MODULE),       B(1)
    };
    // clang-format on

    EXPECT_EQ(actual_bytes, expected_bytes);

    auto pool_obj = pool->get(func_id);
    ASSERT_TRUE(pool_obj->is<std::shared_ptr<Wasp::FunctionBlueprintObject>>());
    auto func_obj = pool_obj->as<std::shared_ptr<Wasp::FunctionBlueprintObject>>();
    const Wasp::CodeObject& inner_code = func_obj->code;

    std::vector<std::byte> actual_inner_bytes(
        inner_code.data(),
        inner_code.data() + inner_code.length()
    );

    // clang-format off
    std::vector<std::byte> expected_inner_bytes = {
        B(Wasp::OpCode::PUSH_SCOPE),
        B(Wasp::OpCode::GET_LOCAL), B(0), // a
        B(Wasp::OpCode::GET_LOCAL), B(1), // b
        B(Wasp::OpCode::ADD),
        B(Wasp::OpCode::RETURN),
        B(Wasp::OpCode::POP_SCOPE),
        B(Wasp::OpCode::LOAD_NONE),
        B(Wasp::OpCode::RETURN)
    };
    // clang-format on

    EXPECT_EQ(actual_inner_bytes, expected_inner_bytes);
}

TEST_F(CompileFunctions, MaxFunction)
{
    auto actual_bytes = compile(R"(
fun max(a: int, b: int) => int
    if a > b then
        return a
    else
        return b
)");

    int max_func_pool_id = pool_size++;
    int max_func_var_id = 0;

    // clang-format off
    std::vector<std::byte> expected_bytes = {
        B(Wasp::OpCode::ENTER_MODULE),

        B(Wasp::OpCode::LOAD_CONST),              B(max_func_pool_id),
        B(Wasp::OpCode::MAKE_FUNCTION),           B(0),
        B(Wasp::OpCode::STORE_FUNCTION_OVERLOAD), B(max_func_var_id),

        B(Wasp::OpCode::JUMP),                    B(10), B(0),

        B(Wasp::OpCode::GET_LOCAL),               B(max_func_var_id),
        B(Wasp::OpCode::EXIT_MODULE),             B(1)
    };
    // clang-format on

    EXPECT_EQ(actual_bytes, expected_bytes);

    auto pool_obj = pool->get(max_func_pool_id);
    ASSERT_TRUE(pool_obj->is<std::shared_ptr<Wasp::FunctionBlueprintObject>>());
    auto func_obj = pool_obj->as<std::shared_ptr<Wasp::FunctionBlueprintObject>>();
    const Wasp::CodeObject& inner_code = func_obj->code;

    std::vector<std::byte> actual_inner_bytes(
        inner_code.data(),
        inner_code.data() + inner_code.length()
    );

    // clang-format off
    std::vector<std::byte> expected_inner_bytes = {
        B(Wasp::OpCode::PUSH_SCOPE),
        B(Wasp::OpCode::JUMP),          B(4), B(0),

        B(Wasp::OpCode::PUSH_SCOPE),
        B(Wasp::OpCode::GET_LOCAL),     B(0),
        B(Wasp::OpCode::GET_LOCAL),     B(1),
        B(Wasp::OpCode::GT),

        B(Wasp::OpCode::JUMP_IF_FALSE), B(23), B(0),
        B(Wasp::OpCode::JUMP),          B(16), B(0),

        B(Wasp::OpCode::GET_LOCAL),     B(0),
        B(Wasp::OpCode::RETURN),
        B(Wasp::OpCode::POP_SCOPE),
        B(Wasp::OpCode::JUMP),          B(32), B(0),

        B(Wasp::OpCode::POP_SCOPE),
        B(Wasp::OpCode::PUSH_SCOPE),
        B(Wasp::OpCode::GET_LOCAL),     B(1),
        B(Wasp::OpCode::RETURN),
        B(Wasp::OpCode::POP_SCOPE),
        B(Wasp::OpCode::JUMP),          B(32), B(0),

        B(Wasp::OpCode::POP_SCOPE),
        B(Wasp::OpCode::LOAD_NONE),
        B(Wasp::OpCode::RETURN)
    };
    // clang-format on

    EXPECT_EQ(actual_inner_bytes, expected_inner_bytes);
}

TEST_F(CompileFunctions, SimpleClosure)
{
    auto actual_bytes = compile(R"(
fun outer(a: int) => any
    fun inner() => int
        return a
    return inner
)");

    int inner_func_pool_id = pool_size++;
    int outer_func_pool_id = pool_size++;
    int outer_func_var_id = 0;

    // clang-format off
    std::vector<std::byte> expected_bytes = {
        B(Wasp::OpCode::ENTER_MODULE),

        B(Wasp::OpCode::LOAD_CONST),        B(outer_func_pool_id),
        B(Wasp::OpCode::MAKE_FUNCTION),     B(0),
        B(Wasp::OpCode::STORE_FUNCTION_OVERLOAD), B(outer_func_var_id),

        B(Wasp::OpCode::JUMP),              B(10), B(0),

        // Export Builder
        B(Wasp::OpCode::GET_LOCAL),         B(outer_func_var_id),
        B(Wasp::OpCode::EXIT_MODULE),       B(1)
    };
    // clang-format on

    EXPECT_EQ(actual_bytes, expected_bytes);

    auto outer_pool_obj = pool->get(outer_func_pool_id);
    ASSERT_TRUE(outer_pool_obj->is<std::shared_ptr<Wasp::FunctionBlueprintObject>>());
    const Wasp::CodeObject&
        outer_code = outer_pool_obj->as<std::shared_ptr<Wasp::FunctionBlueprintObject>>()->code;

    std::vector<std::byte> actual_outer_bytes(
        outer_code.data(),
        outer_code.data() + outer_code.length()
    );

    // clang-format off
    std::vector<std::byte> expected_outer_bytes = {
        B(Wasp::OpCode::PUSH_SCOPE),
        B(Wasp::OpCode::LOAD_CONST),    B(inner_func_pool_id),

        // 1 upval
        B(Wasp::OpCode::MAKE_FUNCTION), B(1),
        B(1), B(0), // is_local=1, idx=0 (It's `a` from the outer params!)

        B(Wasp::OpCode::STORE_FUNCTION_OVERLOAD), B(1), // define inner (Slot 1)

        B(Wasp::OpCode::GET_LOCAL),     B(1),     // return inner
        B(Wasp::OpCode::RETURN),
        B(Wasp::OpCode::POP_SCOPE),
        B(Wasp::OpCode::LOAD_NONE),
        B(Wasp::OpCode::RETURN)
    };
    // clang-format on

    EXPECT_EQ(actual_outer_bytes, expected_outer_bytes);
}
