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

class CompileBranches : public ::testing::Test
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

// ============================================================================
// Control Flow: Branches
// ============================================================================

TEST_F(CompileBranches, IfTernary)
{
    auto actual_bytes = compile(R"(
if false then 25 else 10
)");

    std::vector<std::byte> expected_bytes = {
        /* 0 */ B(Wasp::OpCode::ENTER_MODULE),
        /* 1 */ B(Wasp::OpCode::PUSH_SCOPE), // Scope 1 (Test Scope)
        /* 2 */ B(Wasp::OpCode::LOAD_FALSE),
        /* 3 */ B(Wasp::OpCode::JUMP_IF_FALSE), B(15), B(0),
        /* 6 */ B(Wasp::OpCode::JUMP), B(9), B(0),

        // True Expression
        /* 9 */ B(Wasp::OpCode::LOAD_CONST), B(11), // Value: 25
        /* 11*/ B(Wasp::OpCode::POP_SCOPE),         // Pop Scope 1
        /* 12*/ B(Wasp::OpCode::JUMP), B(23), B(0), // Jump to Converge

        // False Expression
        /* 15*/ B(Wasp::OpCode::POP_SCOPE),         // Pop Scope 1 (Clean up Test Scope)
        /* 16*/ B(Wasp::OpCode::PUSH_SCOPE),        // Scope 2 (Else Branch Scope)
        /* 17*/ B(Wasp::OpCode::LOAD_CONST), B(12), // Value: 10
        /* 19*/ B(Wasp::OpCode::POP_SCOPE),         // Pop Scope 2
        /* 20*/ B(Wasp::OpCode::JUMP), B(23), B(0), // Jump to Converge

        // Converge (End of Statement)
        /* 23*/ B(Wasp::OpCode::POP), // Pop the result (Expression Statement)
        /* 24*/ B(Wasp::OpCode::JUMP), B(27), B(0),
        /* 27*/ B(Wasp::OpCode::EXIT_MODULE)};

    EXPECT_EQ(actual_bytes, expected_bytes);
}

TEST_F(CompileBranches, If)
{
    auto actual_bytes = compile(R"(
if true then
    25 
)");

    std::vector<std::byte> expected_bytes = {
        /* 0 */ B(Wasp::OpCode::ENTER_MODULE),

        /* 1 */ B(Wasp::OpCode::PUSH_SCOPE),

        /* 2 */ B(Wasp::OpCode::LOAD_TRUE),
        /* 3 */ B(Wasp::OpCode::JUMP_IF_FALSE), B(19), B(0),
        /* 6 */ B(Wasp::OpCode::JUMP), B(9), B(0),

        // --- True Block ---
        // Note: No PUSH_SCOPE here because '25' is not a block '{...}'
        /* 9 */ B(Wasp::OpCode::LOAD_CONST), B(11),
        /* 11*/ B(Wasp::OpCode::POP),

        /* 12*/ B(Wasp::OpCode::POP_SCOPE),
        /* 13*/ B(Wasp::OpCode::JUMP), B(16), B(0),

        // --- End Block ---
        /* 16*/ B(Wasp::OpCode::JUMP), B(23), B(0),

        // Exit
        // Cleans up the Condition Scope if test failed
        /* 19*/ B(Wasp::OpCode::POP_SCOPE),
        /* 20*/ B(Wasp::OpCode::JUMP), B(16), B(0),

        /* 23*/ B(Wasp::OpCode::EXIT_MODULE)};

    EXPECT_EQ(actual_bytes, expected_bytes);
}

TEST_F(CompileBranches, IfElse)
{
    auto actual_bytes = compile(R"(
if false then 
    25 
else
    25 
)");

    std::vector<std::byte> expected_bytes = {
        /* 0 */ B(Wasp::OpCode::ENTER_MODULE),

        // 1. Condition Scope (Wraps Test + True Body)
        /* 1 */ B(Wasp::OpCode::PUSH_SCOPE),

        /* 2 */ B(Wasp::OpCode::LOAD_FALSE),
        /* 3 */ B(Wasp::OpCode::JUMP_IF_FALSE), B(19), B(0),
        /* 6 */ B(Wasp::OpCode::JUMP), B(9), B(0),

        // --- True Block ---
        // (No inner PUSH_SCOPE because '25' is an expression, not a block)
        /* 9 */ B(Wasp::OpCode::LOAD_CONST), B(11),
        /* 11*/ B(Wasp::OpCode::POP),

        /* 12*/ B(Wasp::OpCode::POP_SCOPE),
        /* 13*/ B(Wasp::OpCode::JUMP), B(16), B(0),

        // --- End of True ---
        /* 16*/ B(Wasp::OpCode::JUMP), B(28), B(0),

        // --- False Block (Trampoline + Else) ---
        /* 19*/ B(Wasp::OpCode::POP_SCOPE), // 1. Clean up Condition Scope

        /* 20*/ B(Wasp::OpCode::PUSH_SCOPE), // 2. Enter Else Scope
        /* 21*/ B(Wasp::OpCode::LOAD_CONST), B(11),
        /* 23*/ B(Wasp::OpCode::POP),
        /* 24*/ B(Wasp::OpCode::POP_SCOPE), // Pop Else Scope

        /* 25*/ B(Wasp::OpCode::JUMP), B(16), B(0),

        /* 28*/ B(Wasp::OpCode::EXIT_MODULE)};

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

    std::vector<std::byte> expected_bytes = {
        /* 0 */ B(Wasp::OpCode::ENTER_MODULE),

        // --- Outer If ---
        /* 1 */ B(Wasp::OpCode::PUSH_SCOPE), // Scope 1 (Outer)
        /* 2 */ B(Wasp::OpCode::LOAD_FALSE),
        /* 3 */ B(Wasp::OpCode::JUMP_IF_FALSE), B(19), B(0),
        /* 6 */ B(Wasp::OpCode::JUMP), B(9), B(0),

        // Outer True
        /* 9 */ B(Wasp::OpCode::LOAD_CONST), B(11),
        /* 11*/ B(Wasp::OpCode::POP),
        /* 12*/ B(Wasp::OpCode::POP_SCOPE), // Pop Scope 1
        /* 13*/ B(Wasp::OpCode::JUMP), B(16), B(0),

        /* 16*/ B(Wasp::OpCode::JUMP), B(47), B(0), // Jump to Exit

        // --- Outer False (Trampoline) ---
        /* 19*/ B(Wasp::OpCode::POP_SCOPE), // Pop Scope 1

        // --- Elif (Inner If) ---
        /* 20*/ B(Wasp::OpCode::PUSH_SCOPE), // Scope 2 (Elif Condition)
        /* 21*/ B(Wasp::OpCode::LOAD_TRUE),
        /* 22*/ B(Wasp::OpCode::JUMP_IF_FALSE), B(38), B(0),
        /* 25*/ B(Wasp::OpCode::JUMP), B(28), B(0),

        // Elif True
        /* 28*/ B(Wasp::OpCode::LOAD_CONST), B(11),
        /* 30*/ B(Wasp::OpCode::POP),
        /* 31*/ B(Wasp::OpCode::POP_SCOPE), // Pop Scope 2
        /* 32*/ B(Wasp::OpCode::JUMP), B(35), B(0),

        /* 35*/ B(Wasp::OpCode::JUMP), B(16), B(0), // Jump to Outer End (16)

        // --- Elif False (Trampoline) ---
        /* 38*/ B(Wasp::OpCode::POP_SCOPE), // Pop Scope 2

        // --- Else ---
        /* 39*/ B(Wasp::OpCode::PUSH_SCOPE), // Scope 3 (Else)
        /* 40*/ B(Wasp::OpCode::LOAD_CONST), B(11),
        /* 42*/ B(Wasp::OpCode::POP),
        /* 43*/ B(Wasp::OpCode::POP_SCOPE),         // Pop Scope 3
        /* 44*/ B(Wasp::OpCode::JUMP), B(35), B(0), // Jump to Elif End (35)

        /* 47*/ B(Wasp::OpCode::EXIT_MODULE)};

    EXPECT_EQ(actual_bytes, expected_bytes);
}

TEST_F(CompileBranches, IfLet)
{
    auto actual_bytes = compile(R"(
if let x = true then
    25 
)");

    std::vector<std::byte> expected_bytes = {
        /* 0 */ B(Wasp::OpCode::ENTER_MODULE),

        // 1. Condition Scope
        /* 1 */ B(Wasp::OpCode::PUSH_SCOPE),

        // 2. Variable Definition
        /* 2 */ B(Wasp::OpCode::LOAD_TRUE),
        /* 3 */ B(Wasp::OpCode::DUP),
        /* 4 */ B(Wasp::OpCode::DEFINE_LOCAL), B(0),

        /* 6 */ B(Wasp::OpCode::JUMP_IF_FALSE), B(22), B(0),
        /* 9 */ B(Wasp::OpCode::JUMP), B(12), B(0),

        // True Block
        /* 12*/ B(Wasp::OpCode::LOAD_CONST), B(11),
        /* 14*/ B(Wasp::OpCode::POP),
        /* 15*/ B(Wasp::OpCode::POP_SCOPE), // Pop Condition Scope
        /* 16*/ B(Wasp::OpCode::JUMP), B(19), B(0),

        /* 19*/ B(Wasp::OpCode::JUMP), B(26), B(0),

        // False Path (Trampoline)
        /* 22*/ B(Wasp::OpCode::POP_SCOPE), // Pop Condition Scope
        /* 23*/ B(Wasp::OpCode::JUMP), B(19), B(0),

        /* 26*/ B(Wasp::OpCode::EXIT_MODULE)};

    EXPECT_EQ(actual_bytes, expected_bytes);
}
