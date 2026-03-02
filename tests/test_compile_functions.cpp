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

    // ========================================================================
    // 1. Verify Outer Module Bytecode
    // ========================================================================
    std::vector<std::byte> expected_bytes = {
        /* 0 */ B(Wasp::OpCode::ENTER_MODULE),

        // Load blueprint, wrap it, and bind to "add" (Symbol ID 0)
        /* 1 */ B(Wasp::OpCode::LOAD_CONST), B(10),
        /* 3 */ B(Wasp::OpCode::MAKE_FUNCTION),
        /* 4 */ B(Wasp::OpCode::DEFINE_LOCAL), B(0),

        // Jump to exit block
        /* 6 */ B(Wasp::OpCode::JUMP), B(9), B(0),

        /* 9 */ B(Wasp::OpCode::EXIT_MODULE)};

    EXPECT_EQ(actual_bytes, expected_bytes);

    // ========================================================================
    // 2. Extract Function Blueprint from Constant Pool
    // ========================================================================

    // Constant ID 10 is where LOAD_CONST points
    auto pool_obj = current_pool->get(10);

    // Ensure the object actually exists and is a FunctionObject
    ASSERT_TRUE(pool_obj->is<std::shared_ptr<Wasp::FunctionObject>>());
    auto func_obj = pool_obj->as<std::shared_ptr<Wasp::FunctionObject>>();

    const Wasp::CodeObject &inner_code = func_obj->code;

    // ========================================================================
    // 3. Verify Inner Function Bytecode
    // ========================================================================
    std::vector<std::byte> actual_inner_bytes(
        inner_code.data(),
        inner_code.data() + inner_code.length());

    std::vector<std::byte> expected_inner_bytes = {
        /* 0 */ B(Wasp::OpCode::GET_LOCAL), B(0), // Load param 'a' (Symbol ID 0)
        /* 2 */ B(Wasp::OpCode::GET_LOCAL), B(1), // Load param 'b' (Symbol ID 1)
        /* 4 */ B(Wasp::OpCode::ADD),             // Evaluate a + b
        /* 5 */ B(Wasp::OpCode::RETURN),          // Explicit user return

        // Default fallback return automatically injected by compiler
        /* 6 */ B(Wasp::OpCode::LOAD_NONE),
        /* 7 */ B(Wasp::OpCode::RETURN)};

    EXPECT_EQ(actual_inner_bytes, expected_inner_bytes);
}
