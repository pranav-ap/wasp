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

class CompileLoops : public ::testing::Test
{
protected:
    std::string log_dir;
    bool enable_logging = true;

    Wasp::ConstantPool_ptr current_pool;
    Wasp::CodeObject current_bytecode;
    Wasp::CFGraph current_graph;

    void SetUp() override
    {
        const ::testing::TestInfo *const test_info =
            ::testing::UnitTest::GetInstance()->current_test_info();

        std::string suite_name = test_info->test_suite_name();

        log_dir = "/workspaces/wasp/logs/compiler_tests/" + suite_name;

        if (enable_logging && !std::filesystem::exists(log_dir))
        {
            std::filesystem::create_directories(log_dir);
        }

        std::string dots_dir = log_dir + "/dots";

        if (enable_logging && !std::filesystem::exists(dots_dir))
        {
            std::filesystem::create_directories(dots_dir);
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

        std::string dot_file_path = log_dir + "/dots/" + test_name + ".dot";
        std::ofstream dot_file(dot_file_path);

        Wasp::InstructionPrinter printer(current_pool);

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

TEST_F(CompileLoops, WhileLoop)
{
    auto actual_bytes = compile(R"(
while true do
    25
)");

    std::vector<std::byte> expected_bytes = {
        /* 0 */ B(Wasp::OpCode::ENTER_MODULE),
        /* 1 */ B(Wasp::OpCode::JUMP), B(4), B(0),

        // --- Header ---
        // 1. Condition Scope (Wraps everything)
        /* 4 */ B(Wasp::OpCode::PUSH_SCOPE),
        /* 5 */ B(Wasp::OpCode::LOAD_TRUE),
        /* 6 */ B(Wasp::OpCode::JUMP_IF_FALSE), B(21), B(0), // Jump to Trampoline
        /* 9 */ B(Wasp::OpCode::JUMP), B(12), B(0),

        // --- Body ---
        // 2. Body Scope (Wraps just the body)
        /* 12*/ B(Wasp::OpCode::PUSH_SCOPE),
        /* 13*/ B(Wasp::OpCode::LOAD_CONST), B(11), // 25
        /* 15*/ B(Wasp::OpCode::POP),               // Expression cleanup
        /* 16*/ B(Wasp::OpCode::POP_SCOPE),         // Pop Body Scope

        /* 17*/ B(Wasp::OpCode::POP_SCOPE),        // Pop Condition Scope (for loop-back)
        /* 18*/ B(Wasp::OpCode::JUMP), B(4), B(0), // Loop back to Header

        // --- Exit Trampoline ---
        // Cleans up Condition Scope when loop terminates naturally
        /* 21*/ B(Wasp::OpCode::POP_SCOPE),
        /* 22*/ B(Wasp::OpCode::JUMP), B(25), B(0),

        // --- End ---
        /* 25*/ B(Wasp::OpCode::JUMP), B(28), B(0),
        /* 28*/ B(Wasp::OpCode::EXIT_MODULE)};

    EXPECT_EQ(actual_bytes, expected_bytes);
}

TEST_F(CompileLoops, WhileBreak)
{
    auto actual_bytes = compile(R"(
while true do
    25 + 25
    break
)");

    std::vector<std::byte> expected_bytes = {
        /* 0 */ B(Wasp::OpCode::ENTER_MODULE),
        /* 1 */ B(Wasp::OpCode::JUMP), B(4), B(0),

        // --- Header ---
        /* 4 */ B(Wasp::OpCode::PUSH_SCOPE),
        /* 5 */ B(Wasp::OpCode::LOAD_TRUE),
        /* 6 */ B(Wasp::OpCode::JUMP_IF_FALSE), B(29), B(0),
        /* 9 */ B(Wasp::OpCode::JUMP), B(12), B(0),

        // --- Body ---
        /* 12*/ B(Wasp::OpCode::PUSH_SCOPE),

        // Expression: 25 + 25
        /* 13*/ B(Wasp::OpCode::LOAD_CONST), B(11),
        /* 15*/ B(Wasp::OpCode::LOAD_CONST), B(11),
        /* 17*/ B(Wasp::OpCode::ADD),
        /* 18*/ B(Wasp::OpCode::POP),

        // Break Statement
        /* 19*/ B(Wasp::OpCode::POP_SCOPE),
        /* 20*/ B(Wasp::OpCode::POP_SCOPE),
        /* 21*/ B(Wasp::OpCode::JUMP), B(33), B(0),

        /* 24*/ B(Wasp::OpCode::POP_SCOPE),
        /* 25*/ B(Wasp::OpCode::POP_SCOPE),
        /* 26*/ B(Wasp::OpCode::JUMP), B(4), B(0),

        // --- Cleanup ---
        /* 29*/ B(Wasp::OpCode::POP_SCOPE),
        /* 30*/ B(Wasp::OpCode::JUMP), B(33), B(0),

        // --- End ---
        /* 33*/ B(Wasp::OpCode::JUMP), B(36), B(0),
        /* 36*/ B(Wasp::OpCode::EXIT_MODULE)};

    EXPECT_EQ(actual_bytes, expected_bytes);
}

TEST_F(CompileLoops, ForLoop)
{
    auto actual_bytes = compile(R"(
for let x in [1, 2, 3] do
    x 
)");

    std::vector<std::byte> expected_bytes = {
        /* 0 */ B(Wasp::OpCode::ENTER_MODULE),

        /* 1 */ B(Wasp::OpCode::LOAD_CONST), B(11),
        /* 3 */ B(Wasp::OpCode::LOAD_CONST), B(12),
        /* 5 */ B(Wasp::OpCode::LOAD_CONST), B(13),
        /* 7 */ B(Wasp::OpCode::BUILD_LIST), B(3),

        /* 9 */ B(Wasp::OpCode::JUMP), B(12), B(0),

        // --- Header ---
        /* 12*/ B(Wasp::OpCode::LOOP_ITER), B(28), B(0),
        /* 15*/ B(Wasp::OpCode::JUMP), B(18), B(0),

        // --- Body ---
        /* 18*/ B(Wasp::OpCode::PUSH_SCOPE),

        /* 19*/ B(Wasp::OpCode::DEFINE_LOCAL), B(0),
        /* 21*/ B(Wasp::OpCode::GET_LOCAL), B(0),
        /* 23*/ B(Wasp::OpCode::POP),
        /* 24*/ B(Wasp::OpCode::POP_SCOPE),
        /* 25*/ B(Wasp::OpCode::JUMP), B(12), B(0),

        // --- End ---
        /* 26*/ B(Wasp::OpCode::POP),
        /* 25*/ B(Wasp::OpCode::JUMP), B(32), B(0),
        /* 27*/ B(Wasp::OpCode::EXIT_MODULE)};

    EXPECT_EQ(actual_bytes, expected_bytes);
}

TEST_F(CompileLoops, WhileRedo)
{
    auto actual_bytes = compile(R"(
while true do
    25 + 25
    redo
)");

    std::vector<std::byte> expected_bytes = {
        /* 0 */ B(Wasp::OpCode::ENTER_MODULE),
        /* 1 */ B(Wasp::OpCode::JUMP), B(4), B(0),

        // --- Header ---
        /* 4 */ B(Wasp::OpCode::PUSH_SCOPE), // 1. Condition Scope
        /* 5 */ B(Wasp::OpCode::LOAD_TRUE),
        /* 6 */ B(Wasp::OpCode::JUMP_IF_FALSE), B(28), B(0), // Jump to Exit Trampoline
        /* 9 */ B(Wasp::OpCode::JUMP), B(12), B(0),

        // --- Body ---
        /* 12*/ B(Wasp::OpCode::PUSH_SCOPE), // 2. Body Scope

        // Expression: 25 + 25
        /* 13*/ B(Wasp::OpCode::LOAD_CONST), B(11), // 25
        /* 15*/ B(Wasp::OpCode::LOAD_CONST), B(11), // 25
        /* 17*/ B(Wasp::OpCode::ADD),
        /* 18*/ B(Wasp::OpCode::POP), // Expression Statement Cleanup

        // Redo Statement
        /* 19*/ B(Wasp::OpCode::POP_SCOPE),         // Pop ONLY the Body Scope
        /* 20*/ B(Wasp::OpCode::JUMP), B(12), B(0), // Jump directly to Body Block (Offset 12)

        // Normal Loop Continuation (Dead Code due to Redo)
        /* 23*/ B(Wasp::OpCode::POP_SCOPE),        // Pop Body Scope
        /* 24*/ B(Wasp::OpCode::POP_SCOPE),        // Pop Condition Scope
        /* 25*/ B(Wasp::OpCode::JUMP), B(4), B(0), // Jump to Header

        // --- Exit Trampoline (Natural False) ---
        /* 28*/ B(Wasp::OpCode::POP_SCOPE), // Pop Condition Scope
        /* 29*/ B(Wasp::OpCode::JUMP), B(32), B(0),

        // --- End ---
        /* 32*/ B(Wasp::OpCode::JUMP), B(35), B(0),
        /* 35*/ B(Wasp::OpCode::EXIT_MODULE)};

    EXPECT_EQ(actual_bytes, expected_bytes);
}
