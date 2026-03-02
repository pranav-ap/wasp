#include "Token.h"
#include "lexer.h"
#include "parser.h"
#include "SemanticAnalyzer.h"
#include "InstructionPrinter.h"
#include "Compiler.h"
#include "test_utils.h"
#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

class CompilerFunctionsTest : public ::testing::Test
{
protected:
    const std::string log_dir = "/workspaces/wasp/logs/compiler_tests";
    bool enable_logging = true;

    Wasp::ConstantPool_ptr current_pool;
    Wasp::CodeObject current_bytecode;
    Wasp::CFGraph current_graph;

    void SetUp() override
    {
        if (!fs::exists(log_dir))
        {
            fs::create_directories(log_dir);
        }
    }

    static std::byte B(Wasp::OpCode op) { return static_cast<std::byte>(op); }
    static std::byte B(int operand) { return static_cast<std::byte>(operand); }

    std::vector<std::byte> compile(const std::string &source)
    {
        auto mod = parse(source);

        Wasp::SemanticAnalyzer analyzer;
        analyzer.run(mod);

        Wasp::Compiler compiler;
        auto result = compiler.run(mod);

        current_pool = std::get<0>(result);
        current_bytecode = std::get<1>(result);
        current_graph = compiler.get_graph();

        if (enable_logging)
        {
            log();
        }

        const std::byte *data = current_bytecode.data();
        return std::vector<std::byte>(data, data + current_bytecode.length());
    }

    void log()
    {
        const ::testing::TestInfo *const test_info = ::testing::UnitTest::GetInstance()->current_test_info();

        std::string test_name = test_info->name();

        std::string file_path = log_dir + "/" + test_name + ".txt";
        std::ofstream log_file(file_path);

        std::string dot_file_path = log_dir + "/" + test_name + ".dot";
        std::ofstream dot_file(dot_file_path);

        Wasp::InstructionPrinter printer(current_pool);

        if (dot_file.is_open())
        {
            printer.print(current_graph, dot_file);
            dot_file.close();
        }
        else
        {
            FAIL() << "Could not open dot file: " << dot_file_path;
        }

        if (log_file.is_open())
        {
            printer.print(current_bytecode, log_file);
            printer.print_pool(log_file);

            log_file.close();
        }
        else
        {
            FAIL() << "Could not open log file: " << file_path;
        }
    }
};

TEST_F(CompilerFunctionsTest, AddFunction)
{
    auto actual_bytes = compile(R"(
fun add(a: int, b: int) => int
    return a + b
)");

    // Verify Outer Module Bytecode
    std::vector<std::byte> expected_bytes = {
        /* 0 */ B(Wasp::OpCode::ENTER_MODULE),

        /* 1 */ B(Wasp::OpCode::LOAD_CONST), B(10),
        /* 3 */ B(Wasp::OpCode::MAKE_FUNCTION), B(0),
        /* 5 */ B(Wasp::OpCode::DEFINE_LOCAL), B(0),

        // Jump to exit block (Shifted to 10)
        /* 7 */ B(Wasp::OpCode::JUMP), B(10), B(0),

        /* 10*/ B(Wasp::OpCode::EXIT_MODULE)};

    EXPECT_EQ(actual_bytes, expected_bytes);

    // Extract Function Object
    auto pool_obj = current_pool->get(10);

    // Ensure the object actually exists and is a FunctionObject
    ASSERT_TRUE(pool_obj->is<std::shared_ptr<Wasp::FunctionObject>>());
    auto func_obj = pool_obj->as<std::shared_ptr<Wasp::FunctionObject>>();

    const Wasp::CodeObject &inner_code = func_obj->code;

    // Verify Function Bytecode
    std::vector<std::byte> actual_inner_bytes(
        inner_code.data(),
        inner_code.data() + inner_code.length());

    std::vector<std::byte> expected_inner_bytes = {
        /* 0 */ B(Wasp::OpCode::GET_LOCAL), B(0),
        /* 2 */ B(Wasp::OpCode::GET_LOCAL), B(1),
        /* 4 */ B(Wasp::OpCode::ADD),
        /* 5 */ B(Wasp::OpCode::RETURN),

        /* 6 */ B(Wasp::OpCode::LOAD_NONE),
        /* 7 */ B(Wasp::OpCode::RETURN)};

    EXPECT_EQ(actual_inner_bytes, expected_inner_bytes);
}

TEST_F(CompilerFunctionsTest, MaxFunction)
{
    auto actual_bytes = compile(R"(
fun max(a: int, b: int) => int
    if a > b then 
        return a
    else
        return b
)");

    // Verify Outer Module Bytecode
    std::vector<std::byte> expected_bytes = {
        /* 0 */ B(Wasp::OpCode::ENTER_MODULE),

        /* 1 */ B(Wasp::OpCode::LOAD_CONST), B(10),
        /* 3 */ B(Wasp::OpCode::MAKE_FUNCTION), B(0),
        /* 5 */ B(Wasp::OpCode::DEFINE_LOCAL), B(0),

        /* 7 */ B(Wasp::OpCode::JUMP), B(10), B(0),

        /* 10*/ B(Wasp::OpCode::EXIT_MODULE)};

    EXPECT_EQ(actual_bytes, expected_bytes);

    // Extract Function Object
    auto pool_obj = current_pool->get(10);
    ASSERT_TRUE(pool_obj->is<std::shared_ptr<Wasp::FunctionObject>>());
    auto func_obj = pool_obj->as<std::shared_ptr<Wasp::FunctionObject>>();
    const Wasp::CodeObject &inner_code = func_obj->code;

    std::vector<std::byte> actual_inner_bytes(
        inner_code.data(),
        inner_code.data() + inner_code.length());

    // Inner bytes remain completely unchanged
    std::vector<std::byte> expected_inner_bytes = {
        // --- Condition: a > b ---
        /* 0 */ B(Wasp::OpCode::GET_LOCAL), B(0), // a
        /* 2 */ B(Wasp::OpCode::GET_LOCAL), B(1), // b
        /* 4 */ B(Wasp::OpCode::GT),

        // Jump to ELSE block (Offset 21) if false
        /* 5 */ B(Wasp::OpCode::JUMP_IF_FALSE), B(21), B(0),

        // Jump into THEN block
        /* 8 */ B(Wasp::OpCode::JUMP), B(11), B(0),

        // --- THEN Block ---
        /* 11 */ B(Wasp::OpCode::PUSH_SCOPE),
        /* 12 */ B(Wasp::OpCode::GET_LOCAL), B(0),
        /* 14 */ B(Wasp::OpCode::RETURN),
        /* 15 */ B(Wasp::OpCode::POP_SCOPE),
        /* 16 */ B(Wasp::OpCode::JUMP), B(19), B(0),

        // --- Failsafe Return Block ---
        /* 19 */ B(Wasp::OpCode::LOAD_NONE),
        /* 20 */ B(Wasp::OpCode::RETURN),

        // --- ELSE Block ---
        /* 21 */ B(Wasp::OpCode::PUSH_SCOPE),
        /* 22 */ B(Wasp::OpCode::GET_LOCAL), B(1),
        /* 24 */ B(Wasp::OpCode::RETURN),
        /* 25 */ B(Wasp::OpCode::POP_SCOPE),
        /* 26 */ B(Wasp::OpCode::JUMP), B(19), B(0)};

    EXPECT_EQ(actual_inner_bytes, expected_inner_bytes);
}

TEST_F(CompilerFunctionsTest, CallFunction)
{
    auto actual_bytes = compile(R"(
fun add(a: int, b: int) => int
    return a + b

let result = add(10, 20)
)");

    std::vector<std::byte> expected_bytes = {
        /* 0 */ B(Wasp::OpCode::ENTER_MODULE),

        /* 1 */ B(Wasp::OpCode::LOAD_CONST), B(10),
        /* 3 */ B(Wasp::OpCode::MAKE_FUNCTION), B(0),
        /* 5 */ B(Wasp::OpCode::DEFINE_LOCAL), B(0),

        /* 7 */ B(Wasp::OpCode::GET_LOCAL), B(0),
        /* 9 */ B(Wasp::OpCode::LOAD_CONST), B(11),
        /* 11*/ B(Wasp::OpCode::LOAD_CONST), B(12),
        /* 13*/ B(Wasp::OpCode::CALL), B(2),

        /* 15*/ B(Wasp::OpCode::DEFINE_LOCAL), B(1),

        /* 17*/ B(Wasp::OpCode::JUMP), B(20), B(0),

        /* 20*/ B(Wasp::OpCode::EXIT_MODULE)};

    EXPECT_EQ(actual_bytes, expected_bytes);
}

TEST_F(CompilerFunctionsTest, SimpleClosure)
{
    auto actual_bytes = compile(R"(
fun outer(a: int)
    fun inner()
        return a
    return inner
)");

    // ========================================================================
    // 1. Verify Outer Module (Creates 'outer')
    // ========================================================================
    std::vector<std::byte> expected_bytes = {
        /* 0 */ B(Wasp::OpCode::ENTER_MODULE),

        // Load blueprint for 'outer' (ID 11), 0 upvalues, bind to 'outer' (Symbol 0)
        /* 1 */ B(Wasp::OpCode::LOAD_CONST), B(11),
        /* 3 */ B(Wasp::OpCode::MAKE_FUNCTION), B(0),
        /* 5 */ B(Wasp::OpCode::DEFINE_LOCAL), B(0),

        /* 7 */ B(Wasp::OpCode::JUMP), B(10), B(0),
        /* 10*/ B(Wasp::OpCode::EXIT_MODULE)};

    EXPECT_EQ(actual_bytes, expected_bytes);

    // ========================================================================
    // 2. Verify Outer Function (Creates 'inner' and captures 'a')
    // ========================================================================
    auto outer_pool_obj = current_pool->get(11);
    ASSERT_TRUE(outer_pool_obj->is<std::shared_ptr<Wasp::FunctionObject>>());
    const Wasp::CodeObject &outer_code = outer_pool_obj->as<std::shared_ptr<Wasp::FunctionObject>>()->code;

    std::vector<std::byte> actual_outer_bytes(outer_code.data(), outer_code.data() + outer_code.length());

    std::vector<std::byte> expected_outer_bytes = {
        // Load blueprint for 'inner' (ID 10)
        /* 0 */ B(Wasp::OpCode::LOAD_CONST), B(10),

        // MAKE_FUNCTION with 1 upvalue
        /* 2 */ B(Wasp::OpCode::MAKE_FUNCTION), B(1),
        /* 4 */ B(1), // Breadcrumb: is_local = true (grab from stack)
        /* 5 */ B(0), // Breadcrumb: index = 0 (grab parameter 'a')

        /* 6 */ B(Wasp::OpCode::DEFINE_LOCAL), B(1), // Bind to 'inner' (Symbol 1)

        // return inner
        /* 8 */ B(Wasp::OpCode::GET_LOCAL), B(1),
        /* 10*/ B(Wasp::OpCode::RETURN),

        // Failsafe return
        /* 11*/ B(Wasp::OpCode::LOAD_NONE),
        /* 12*/ B(Wasp::OpCode::RETURN)};

    EXPECT_EQ(actual_outer_bytes, expected_outer_bytes);

    // ========================================================================
    // 3. Verify Inner Function (Reads from the backpack)
    // ========================================================================
    auto inner_pool_obj = current_pool->get(10);
    ASSERT_TRUE(inner_pool_obj->is<std::shared_ptr<Wasp::FunctionObject>>());
    const Wasp::CodeObject &inner_code = inner_pool_obj->as<std::shared_ptr<Wasp::FunctionObject>>()->code;

    std::vector<std::byte> actual_inner_bytes(inner_code.data(), inner_code.data() + inner_code.length());

    std::vector<std::byte> expected_inner_bytes = {
        // return a (Reads from the closure backpack at index 0)
        /* 0 */ B(Wasp::OpCode::GET_UPVALUE), B(0),
        /* 2 */ B(Wasp::OpCode::RETURN),

        // Failsafe return
        /* 3 */ B(Wasp::OpCode::LOAD_NONE),
        /* 4 */ B(Wasp::OpCode::RETURN)};

    EXPECT_EQ(actual_inner_bytes, expected_inner_bytes);
}
