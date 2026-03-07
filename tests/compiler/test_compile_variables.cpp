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

class CompileVariables : public ::testing::Test
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
TEST_F(CompileVariables, DefineAndUseVariable)
{
    auto actual_bytes = compile(R"(
let x = 42
x + 1
)");

    std::vector<std::byte> expected_bytes = {
        /* 0 */ B(Wasp::OpCode::ENTER_MODULE),
        /* 1 */ B(Wasp::OpCode::LOAD_CONST), B(11), // 42
        /* 3 */ B(Wasp::OpCode::DEFINE_LOCAL), B(1),

        /* 5 */ B(Wasp::OpCode::GET_GLOBAL), B(1),  // "x"
        /* 7 */ B(Wasp::OpCode::LOAD_CONST), B(12), // 1
        /* 9 */ B(Wasp::OpCode::ADD),
        /* 10*/ B(Wasp::OpCode::POP),
        /* 11*/ B(Wasp::OpCode::JUMP), B(14), B(0),
        /* 14*/ B(Wasp::OpCode::EXIT_MODULE)};

    EXPECT_EQ(actual_bytes, expected_bytes);
}
TEST_F(CompileVariables, DefineAndReAssignVariable)
{
    auto actual_bytes = compile(R"(
let x = 42
x = x + 1
)");

    std::vector<std::byte> expected_bytes = {
        /* 0 */ B(Wasp::OpCode::ENTER_MODULE),
        /* 1 */ B(Wasp::OpCode::LOAD_CONST), B(11),  // 42
        /* 3 */ B(Wasp::OpCode::DEFINE_LOCAL), B(1), // Local Slot 0 (Not a pool index!)

        /* 5 */ B(Wasp::OpCode::GET_GLOBAL), B(1),  // "x"
        /* 7 */ B(Wasp::OpCode::LOAD_CONST), B(12), // 1
        /* 9 */ B(Wasp::OpCode::ADD),
        /* 10*/ B(Wasp::OpCode::SET_GLOBAL), B(1), // "x"
        /* 12*/ B(Wasp::OpCode::POP),
        /* 13*/ B(Wasp::OpCode::JUMP), B(16), B(0),
        /* 16*/ B(Wasp::OpCode::EXIT_MODULE)};

    EXPECT_EQ(actual_bytes, expected_bytes);
}
