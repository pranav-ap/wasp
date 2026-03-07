#include "Token.h"
#include "lexer.h"
#include "parser.h"
#include "NativeRegistry.h"
#include "SemanticAnalyzer.h"
#include "InstructionPrinter.h"
#include "Compiler.h"
#include "test_utils.h"
#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

class CompileFunctions : public ::testing::Test
{
protected:
    std::string log_dir;
    bool enable_logging = true;

    Wasp::ConstantPool_ptr pool;
    Wasp::CodeObject current_bytecode;
    Wasp::CFGraph current_graph;

    void SetUp() override
    {
        const ::testing::TestInfo *const test_info =
            ::testing::UnitTest::GetInstance()->current_test_info();

        std::string suite_name = test_info->test_suite_name();

        log_dir = "/workspaces/wasp/logs/compiler_tests/" + suite_name;

        if (enable_logging)
        {
            if (!std::filesystem::exists(log_dir))
            {
                std::filesystem::create_directories(log_dir);
            }

            // Create 'dots' subdirectory
            std::string dots_dir = log_dir + "/dots";
            if (!std::filesystem::exists(dots_dir))
            {
                std::filesystem::create_directories(dots_dir);
            }
        }
    }

    static std::byte B(Wasp::OpCode op) { return static_cast<std::byte>(op); }
    static std::byte B(int operand) { return static_cast<std::byte>(operand); }

    std::vector<std::byte> compile(const std::string &source)
    {
        auto mod = parse(source);

        pool = std::make_shared<Wasp::ConstantPool>();

        auto native_registry = std::make_shared<Wasp::NativeRegistry>(pool);
        native_registry->load_stdlib();

        auto semantic_analyzer = Wasp::SemanticAnalyzer(native_registry);
        semantic_analyzer.run(mod);

        Wasp::Compiler compiler(pool);
        current_bytecode = compiler.run(mod);
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

        std::string dot_file_path = log_dir + "/dots/" + test_name + ".dot";
        std::ofstream dot_file(dot_file_path);

        Wasp::InstructionPrinter printer(pool);

        if (dot_file.is_open())
        {
            printer.print(current_graph, dot_file);
            dot_file.close();
        }

        if (log_file.is_open())
        {
            printer.print(current_bytecode, log_file);
            printer.print_pool(log_file);
            log_file.close();
        }
    }
};

TEST_F(CompileFunctions, AddFunction)
{
    auto actual_bytes = compile(R"(
fun add(a: int, b: int) => int
    return a + b
)");

    // Verify Outer Module Bytecode
    std::vector<std::byte> expected_bytes = {
        /* 0 */ B(Wasp::OpCode::ENTER_MODULE),
        /* 1 */ B(Wasp::OpCode::LOAD_CONST), B(11),
        /* 3 */ B(Wasp::OpCode::MAKE_FUNCTION), B(0),
        /* 5 */ B(Wasp::OpCode::DEFINE_LOCAL), B(1),
        /* 7 */ B(Wasp::OpCode::JUMP), B(10), B(0),
        /* 10*/ B(Wasp::OpCode::EXIT_MODULE)};

    EXPECT_EQ(actual_bytes, expected_bytes);

    // Extract Function Object
    auto pool_obj = pool->get(11);
    ASSERT_TRUE(pool_obj->is<std::shared_ptr<Wasp::FunctionObject>>());
    auto func_obj = pool_obj->as<std::shared_ptr<Wasp::FunctionObject>>();
    const Wasp::CodeObject &inner_code = func_obj->code;

    std::vector<std::byte> actual_inner_bytes(inner_code.data(), inner_code.data() + inner_code.length());

    std::vector<std::byte> expected_inner_bytes = {
        /* 0 */ B(Wasp::OpCode::PUSH_SCOPE),
        /* 1 */ B(Wasp::OpCode::GET_LOCAL), B(0),
        /* 3 */ B(Wasp::OpCode::GET_LOCAL), B(1),
        /* 5 */ B(Wasp::OpCode::ADD),
        /* 7 */ B(Wasp::OpCode::RETURN),

        // can be ignored as return will take care of pop scope
        /* 6 */ B(Wasp::OpCode::POP_SCOPE),

        // implicit return can also be ignored
        /* 8 */ B(Wasp::OpCode::LOAD_NONE),
        /* 9 */ B(Wasp::OpCode::RETURN)};

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

    // Verify Outer Module Bytecode
    std::vector<std::byte> expected_bytes = {
        /* 0 */ B(Wasp::OpCode::ENTER_MODULE),
        /* 1 */ B(Wasp::OpCode::LOAD_CONST), B(11),
        /* 3 */ B(Wasp::OpCode::MAKE_FUNCTION), B(0),
        /* 5 */ B(Wasp::OpCode::DEFINE_LOCAL), B(1),
        /* 7 */ B(Wasp::OpCode::JUMP), B(10), B(0),
        /* 10*/ B(Wasp::OpCode::EXIT_MODULE)};

    EXPECT_EQ(actual_bytes, expected_bytes);

    // Extract Function Object
    auto pool_obj = pool->get(11);
    ASSERT_TRUE(pool_obj->is<std::shared_ptr<Wasp::FunctionObject>>());
    auto func_obj = pool_obj->as<std::shared_ptr<Wasp::FunctionObject>>();
    const Wasp::CodeObject &inner_code = func_obj->code;

    std::vector<std::byte> actual_inner_bytes(inner_code.data(), inner_code.data() + inner_code.length());

    std::vector<std::byte> expected_inner_bytes = {
        /* 0 */ B(Wasp::OpCode::PUSH_SCOPE), // Function Scope

        // --- If Condition ---
        /* 1 */ B(Wasp::OpCode::PUSH_SCOPE), // If Statement Scope (wraps condition + body)
        /* 2 */ B(Wasp::OpCode::GET_LOCAL), B(0),
        /* 4 */ B(Wasp::OpCode::GET_LOCAL), B(1),
        /* 6 */ B(Wasp::OpCode::JUMP_IF_FALSE), B(22), B(0),
        /* 9 */ B(Wasp::OpCode::JUMP), B(12), B(0),

        // --- THEN Block ---
        // Notice no POP_SCOPE before RETURN, VM handles it.
        /* 12*/ B(Wasp::OpCode::GET_LOCAL), B(0),
        /* 14*/ B(Wasp::OpCode::RETURN),

        // Dead code (If cleanup path)
        /* 15*/ B(Wasp::OpCode::POP_SCOPE),
        /* 16*/ B(Wasp::OpCode::JUMP), B(19), B(0),

        // Dead code (End of function exit path)
        /* 19*/ B(Wasp::OpCode::POP_SCOPE),
        /* 20*/ B(Wasp::OpCode::LOAD_NONE),
        /* 21*/ B(Wasp::OpCode::RETURN),

        // --- ELSE Block (Trampoline) ---
        /* 22*/ B(Wasp::OpCode::POP_SCOPE), // Pop Condition Scope

        /* 23*/ B(Wasp::OpCode::PUSH_SCOPE), // Else Scope
        /* 24*/ B(Wasp::OpCode::GET_LOCAL), B(1),
        /* 26*/ B(Wasp::OpCode::RETURN),

        // Dead code (Else cleanup path)
        /* 27*/ B(Wasp::OpCode::POP_SCOPE),
        /* 28*/ B(Wasp::OpCode::JUMP), B(19), B(0)};

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

    // ========================================================================
    // 1. Verify Outer Module (Creates 'outer')
    // ========================================================================
    std::vector<std::byte> expected_bytes = {
        /* 0 */ B(Wasp::OpCode::ENTER_MODULE),
        /* 1 */ B(Wasp::OpCode::LOAD_CONST), B(12), // 'outer' FunctionObject
        /* 3 */ B(Wasp::OpCode::MAKE_FUNCTION), B(0),
        /* 5 */ B(Wasp::OpCode::DEFINE_LOCAL), B(1),
        /* 7 */ B(Wasp::OpCode::JUMP), B(10), B(0),
        /* 10*/ B(Wasp::OpCode::EXIT_MODULE)};

    EXPECT_EQ(actual_bytes, expected_bytes);
    // ========================================================================
    // 2. Verify Outer Function (Creates 'inner' and captures 'a')
    // ========================================================================
    auto outer_pool_obj = pool->get(12);
    ASSERT_TRUE(outer_pool_obj->is<std::shared_ptr<Wasp::FunctionObject>>());
    const Wasp::CodeObject &outer_code = outer_pool_obj->as<std::shared_ptr<Wasp::FunctionObject>>()->code;

    std::vector<std::byte> actual_outer_bytes(outer_code.data(), outer_code.data() + outer_code.length());

    std::vector<std::byte> expected_outer_bytes = {
        /* 0 */ B(Wasp::OpCode::PUSH_SCOPE),

        /* 1 */ B(Wasp::OpCode::LOAD_CONST), B(11), // 'inner' FunctionObject

        // [FIXED] Now expects 1 upvalue!
        /* 3 */ B(Wasp::OpCode::MAKE_FUNCTION), B(1),

        // Upvalue 0: Capture 'a' (is_local=1, index=0)
        /* 5 */ B(1),
        /* 6 */ B(0),

        /* 7 */ B(Wasp::OpCode::DEFINE_LOCAL), B(1), // Define 'inner' at Index 1 ('a' is 0)

        // return inner
        /* 9 */ B(Wasp::OpCode::GET_LOCAL), B(1),
        /* 11*/ B(Wasp::OpCode::RETURN),

        // Implicit Return
        /* 12*/ B(Wasp::OpCode::POP_SCOPE),
        /* 13*/ B(Wasp::OpCode::LOAD_NONE),
        /* 14*/ B(Wasp::OpCode::RETURN)};

    EXPECT_EQ(actual_outer_bytes, expected_outer_bytes);
}

TEST_F(CompileFunctions, Print)
{
    auto actual_bytes = compile(R"(
print(1)
)");

    std::vector<std::byte> expected_bytes = {
        /* 0 */ B(Wasp::OpCode::ENTER_MODULE),
        /* 1 */ B(Wasp::OpCode::GET_GLOBAL), B(0),
        /* 3 */ B(Wasp::OpCode::LOAD_CONST), B(11),
        /* 5 */ B(Wasp::OpCode::CALL), B(1),
        /* 7 */ B(Wasp::OpCode::POP),
        /* 8 */ B(Wasp::OpCode::JUMP), B(11), B(0),
        /* 11 */ B(Wasp::OpCode::EXIT_MODULE)};

    EXPECT_EQ(actual_bytes, expected_bytes);
}