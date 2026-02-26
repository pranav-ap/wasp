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

class CompilerTest : public ::testing::Test
{
protected:
    const std::string log_dir = "/workspaces/wasp/logs/compiler_tests";
    bool enable_logging = true;

    Wasp::ConstantPool_ptr current_pool;
    std::map<int, std::string> current_name_map;
    Wasp::CodeObject current_bytecode;

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
        current_name_map = compiler.get_name_map();

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

        if (log_file.is_open())
        {
            Wasp::InstructionPrinter printer(current_pool, current_name_map);
            printer.print(current_bytecode, log_file);
            log_file.close();
        }
        else
        {
            FAIL() << "Could not open log file: " << file_path;
        }
    }
};

TEST_F(CompilerTest, SimpleInteger)
{
    auto actual_bytes = compile("25");

    std::vector<std::byte> expected_bytes = {
        B(Wasp::OpCode::ENTER_MODULE),
        B(Wasp::OpCode::LOAD_CONST), B(10),
        B(Wasp::OpCode::POP),
        B(Wasp::OpCode::EXIT_MODULE)};

    EXPECT_EQ(actual_bytes, expected_bytes);
}

TEST_F(CompilerTest, SimpleList)
{
    auto actual_bytes = compile("[1, 2, 3]");

    std::vector<std::byte> expected_bytes = {
        B(Wasp::OpCode::ENTER_MODULE),
        B(Wasp::OpCode::LOAD_CONST), B(10),
        B(Wasp::OpCode::LOAD_CONST), B(11),
        B(Wasp::OpCode::LOAD_CONST), B(12),
        B(Wasp::OpCode::BUILD_LIST), B(3),
        B(Wasp::OpCode::POP),
        B(Wasp::OpCode::EXIT_MODULE)};

    EXPECT_EQ(actual_bytes, expected_bytes);
}

TEST_F(CompilerTest, NegateNumber)
{
    auto actual_bytes = compile("-2");

    std::vector<std::byte> expected_bytes = {
        B(Wasp::OpCode::ENTER_MODULE),
        B(Wasp::OpCode::LOAD_CONST), B(10),
        B(Wasp::OpCode::NEGATE),
        B(Wasp::OpCode::POP),
        B(Wasp::OpCode::EXIT_MODULE)};

    EXPECT_EQ(actual_bytes, expected_bytes);
}

TEST_F(CompilerTest, SimpleAddition)
{
    auto actual_bytes = compile("1 + 2");

    std::vector<std::byte> expected_bytes = {
        B(Wasp::OpCode::ENTER_MODULE),
        B(Wasp::OpCode::LOAD_CONST), B(10),
        B(Wasp::OpCode::LOAD_CONST), B(11),
        B(Wasp::OpCode::ADD),
        B(Wasp::OpCode::POP),
        B(Wasp::OpCode::EXIT_MODULE)};

    EXPECT_EQ(actual_bytes, expected_bytes);
}

TEST_F(CompilerTest, DefineAndUseVariable)
{
    auto actual_bytes = compile(R"(
let x = 42
x + 1
)");

    std::vector<std::byte> expected_bytes = {
        B(Wasp::OpCode::ENTER_MODULE),

        B(Wasp::OpCode::LOAD_CONST), B(10),
        B(Wasp::OpCode::DEFINE_LOCAL), B(0),

        B(Wasp::OpCode::GET_LOCAL), B(0),
        B(Wasp::OpCode::LOAD_CONST), B(11),
        B(Wasp::OpCode::ADD),
        B(Wasp::OpCode::POP),

        B(Wasp::OpCode::EXIT_MODULE)};

    EXPECT_EQ(actual_bytes, expected_bytes);
}

TEST_F(CompilerTest, DefineAndReAssignVariable)
{
    auto actual_bytes = compile(R"(
let x = 42
x = x + 1
)");

    std::vector<std::byte> expected_bytes = {
        B(Wasp::OpCode::ENTER_MODULE),

        B(Wasp::OpCode::LOAD_CONST), B(10),
        B(Wasp::OpCode::DEFINE_LOCAL), B(0),

        B(Wasp::OpCode::GET_LOCAL), B(0),
        B(Wasp::OpCode::LOAD_CONST), B(11),
        B(Wasp::OpCode::ADD),
        B(Wasp::OpCode::SET_LOCAL), B(0),
        B(Wasp::OpCode::POP),

        B(Wasp::OpCode::EXIT_MODULE)};

    EXPECT_EQ(actual_bytes, expected_bytes);
}

TEST_F(CompilerTest, IfStatementNoElse)
{
    auto actual_bytes = compile(R"(
if true then
    25 
)");

    std::vector<std::byte> expected_bytes = {
        B(Wasp::OpCode::ENTER_MODULE), // 0

        B(Wasp::OpCode::LOAD_TRUE),           // 1
        B(Wasp::OpCode::JUMP_IF_FALSE), B(9), // 2 (Jumps to End)

        B(Wasp::OpCode::LOAD_CONST), B(10), // 4
        B(Wasp::OpCode::POP),               // 6
        B(Wasp::OpCode::JUMP), B(9),        // 7 (Jumps to End)

        B(Wasp::OpCode::EXIT_MODULE) // 9
    };

    EXPECT_EQ(actual_bytes, expected_bytes);
}

TEST_F(CompilerTest, IfStatementWithElse)
{
    auto actual_bytes = compile(R"(
if false then 
    25 
else
    25 
)");

    std::vector<std::byte> expected_bytes = {
        B(Wasp::OpCode::ENTER_MODULE), // 0

        B(Wasp::OpCode::LOAD_FALSE),          // 1
        B(Wasp::OpCode::JUMP_IF_FALSE), B(9), // 2 (Jumps to False block)

        // True Block
        B(Wasp::OpCode::LOAD_CONST), B(10), // 4
        B(Wasp::OpCode::POP),               // 6
        B(Wasp::OpCode::JUMP), B(14),       // 7 (Jumps to End)

        // False Block
        B(Wasp::OpCode::LOAD_CONST), B(10), // 9
        B(Wasp::OpCode::POP),               // 11
        B(Wasp::OpCode::JUMP), B(14),       // 12 (Jumps to End)

        // End Block
        B(Wasp::OpCode::EXIT_MODULE) // 14
    };

    EXPECT_EQ(actual_bytes, expected_bytes);
}