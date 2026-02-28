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
        B(Wasp::OpCode::JUMP), B(6),
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
        B(Wasp::OpCode::JUMP), B(12),
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
        B(Wasp::OpCode::JUMP), B(7),
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
        B(Wasp::OpCode::JUMP), B(9),
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
        B(Wasp::OpCode::JUMP), B(13),

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
        B(Wasp::OpCode::JUMP), B(15),

        B(Wasp::OpCode::EXIT_MODULE)};

    EXPECT_EQ(actual_bytes, expected_bytes);
}

TEST_F(CompilerTest, If)
{
    auto actual_bytes = compile(R"(
if true then
    25 
)");

    std::vector<std::byte> expected_bytes = {
        /* 0 */ B(Wasp::OpCode::ENTER_MODULE),

        /* 1 */ B(Wasp::OpCode::LOAD_TRUE),
        /* 2 */ B(Wasp::OpCode::JUMP_IF_FALSE), B(11),
        /* 4 */ B(Wasp::OpCode::JUMP), B(6),

        // True Block
        /* 6 */ B(Wasp::OpCode::LOAD_CONST), B(10),
        /* 8 */ B(Wasp::OpCode::POP),
        /* 9 */ B(Wasp::OpCode::JUMP), B(11),

        // Reconvergance/End Block
        /* 11 */ B(Wasp::OpCode::JUMP), B(13),

        /* 13 */ B(Wasp::OpCode::EXIT_MODULE)};

    EXPECT_EQ(actual_bytes, expected_bytes);
}

TEST_F(CompilerTest, IfElse)
{
    auto actual_bytes = compile(R"(
if false then 
    25 
else
    25 
)");

    std::vector<std::byte> expected_bytes = {
        /* 0 */ B(Wasp::OpCode::ENTER_MODULE),

        /* 1 */ B(Wasp::OpCode::LOAD_FALSE),
        /* 2 */ B(Wasp::OpCode::JUMP_IF_FALSE), B(13),
        /* 4 */ B(Wasp::OpCode::JUMP), B(6),

        // True Block
        /* 6 */ B(Wasp::OpCode::LOAD_CONST), B(10),
        /* 8 */ B(Wasp::OpCode::POP),
        /* 9 */ B(Wasp::OpCode::JUMP), B(11),

        // End Block / Jump to Exit
        /* 11 */ B(Wasp::OpCode::JUMP), B(18),

        // False Block
        /* 13 */ B(Wasp::OpCode::LOAD_CONST), B(10),
        /* 15 */ B(Wasp::OpCode::POP),
        /* 16 */ B(Wasp::OpCode::JUMP), B(11),

        /* 18 */ B(Wasp::OpCode::EXIT_MODULE)};

    EXPECT_EQ(actual_bytes, expected_bytes);
}

TEST_F(CompilerTest, IfElIfElse)
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
        /* 1 */ B(Wasp::OpCode::LOAD_FALSE),
        /* 2 */ B(Wasp::OpCode::JUMP_IF_FALSE), B(13),
        /* 4 */ B(Wasp::OpCode::JUMP), B(6),
        /* 6 */ B(Wasp::OpCode::LOAD_CONST), B(10),
        /* 8 */ B(Wasp::OpCode::POP),
        /* 9 */ B(Wasp::OpCode::JUMP), B(11),
        /* 11 */ B(Wasp::OpCode::JUMP), B(30),
        /* 13 */ B(Wasp::OpCode::LOAD_TRUE),
        /* 14 */ B(Wasp::OpCode::JUMP_IF_FALSE), B(25),
        /* 16 */ B(Wasp::OpCode::JUMP), B(18),
        /* 18 */ B(Wasp::OpCode::LOAD_CONST), B(10),
        /* 20 */ B(Wasp::OpCode::POP),
        /* 21 */ B(Wasp::OpCode::JUMP), B(23),
        /* 23 */ B(Wasp::OpCode::JUMP), B(11),
        /* 25 */ B(Wasp::OpCode::LOAD_CONST), B(10),
        /* 27 */ B(Wasp::OpCode::POP),
        /* 28 */ B(Wasp::OpCode::JUMP), B(23),
        /* 30 */ B(Wasp::OpCode::EXIT_MODULE)};

    EXPECT_EQ(actual_bytes, expected_bytes);
}

TEST_F(CompilerTest, IfTernary)
{
    auto actual_bytes = compile(R"(
if false then 25 else 10
)");

    std::vector<std::byte> expected_bytes = {
        /* 0 */ B(Wasp::OpCode::ENTER_MODULE),

        /* 1 */ B(Wasp::OpCode::LOAD_FALSE),
        /* 2 */ B(Wasp::OpCode::JUMP_IF_FALSE), B(8),

        // True Expression
        /* 4 */ B(Wasp::OpCode::LOAD_CONST), B(10), // (25)
        /* 6 */ B(Wasp::OpCode::JUMP), B(12),

        // False Expression
        /* 8 */ B(Wasp::OpCode::LOAD_CONST), B(11), // (10)
        /* 10 */ B(Wasp::OpCode::JUMP), B(12),

        // End of Ternary (Expression Statement Pop)
        /* 12 */ B(Wasp::OpCode::POP),

        /* 13 */ B(Wasp::OpCode::JUMP), B(15),
        /* 15 */ B(Wasp::OpCode::EXIT_MODULE)};

    EXPECT_EQ(actual_bytes, expected_bytes);
}

TEST_F(CompilerTest, IfLet)
{
    auto actual_bytes = compile(R"(
if let x = true then
    25 
)");

    std::vector<std::byte> expected_bytes = {
        /* 0 */ B(Wasp::OpCode::ENTER_MODULE),

        /* 1 */ B(Wasp::OpCode::LOAD_TRUE),

        // Duplicate so we can bind one and check the other
        /* 2 */ B(Wasp::OpCode::DUP),

        /* 3 */ B(Wasp::OpCode::DEFINE_LOCAL), B(0),

        /* 5 */ B(Wasp::OpCode::JUMP_IF_FALSE), B(14),
        /* 7 */ B(Wasp::OpCode::JUMP), B(9),

        /* 9 */ B(Wasp::OpCode::LOAD_CONST), B(10), // (25)
        /* 11 */ B(Wasp::OpCode::POP),

        /* 12 */ B(Wasp::OpCode::JUMP), B(14),
        /* 14 */ B(Wasp::OpCode::JUMP), B(16),

        /* 16 */ B(Wasp::OpCode::EXIT_MODULE)};

    EXPECT_EQ(actual_bytes, expected_bytes);
}

TEST_F(CompilerTest, WhileLoop)
{
    auto actual_bytes = compile(R"(
while true do
    25
)");

    std::vector<std::byte> expected_bytes = {
        /* 0 */ B(Wasp::OpCode::ENTER_MODULE),

        // Header Block
        /* 1 */ B(Wasp::OpCode::JUMP), B(3),
        /* 3 */ B(Wasp::OpCode::LOAD_TRUE),
        /* 4 */ B(Wasp::OpCode::JUMP_IF_FALSE), B(13),
        /* 6 */ B(Wasp::OpCode::JUMP), B(8),

        // Body
        /* 8 */ B(Wasp::OpCode::LOAD_CONST), B(10), // (25)
        /* 10 */ B(Wasp::OpCode::POP),

        /* 11 */ B(Wasp::OpCode::JUMP), B(3),
        /* 13 */ B(Wasp::OpCode::JUMP), B(15),

        /* 15 */ B(Wasp::OpCode::EXIT_MODULE)};

    EXPECT_EQ(actual_bytes, expected_bytes);
}

TEST_F(CompilerTest, UntilLoop)
{
    auto actual_bytes = compile(R"(
until true do
    25
)");

    std::vector<std::byte> expected_bytes = {
        /* 0 */ B(Wasp::OpCode::ENTER_MODULE),

        // Header
        /* 1 */ B(Wasp::OpCode::JUMP), B(3),
        /* 3 */ B(Wasp::OpCode::LOAD_TRUE),
        /* 4 */ B(Wasp::OpCode::NOT), // Inverts the condition
        /* 5 */ B(Wasp::OpCode::JUMP_IF_FALSE), B(14),
        /* 7 */ B(Wasp::OpCode::JUMP), B(9),

        // Body
        /* 9 */ B(Wasp::OpCode::LOAD_CONST), B(10), // (25)
        /* 11 */ B(Wasp::OpCode::POP),

        /* 12 */ B(Wasp::OpCode::JUMP), B(3),
        /* 14 */ B(Wasp::OpCode::JUMP), B(16),

        /* 16 */ B(Wasp::OpCode::EXIT_MODULE)};

    EXPECT_EQ(actual_bytes, expected_bytes);
}

TEST_F(CompilerTest, ForLoop)
{
    auto actual_bytes = compile(R"(
for let x in [1, 2, 3] do
    x 
)");

    std::vector<std::byte> expected_bytes = {
        /* 0 */ B(Wasp::OpCode::ENTER_MODULE),

        /* 1 */ B(Wasp::OpCode::LOAD_CONST), B(10), // (1)
        /* 3 */ B(Wasp::OpCode::LOAD_CONST), B(11), // (2)
        /* 5 */ B(Wasp::OpCode::LOAD_CONST), B(12), // (3)
        /* 7 */ B(Wasp::OpCode::BUILD_LIST), B(3),

        /* 9 */ B(Wasp::OpCode::JUMP), B(11),

        // --------------------------------------------------------------------
        // Header
        // --------------------------------------------------------------------
        /* 11 */ B(Wasp::OpCode::LOOP_ITER), B(22),
        /* 13 */ B(Wasp::OpCode::JUMP), B(15),

        // --------------------------------------------------------------------
        // Body
        // --------------------------------------------------------------------
        /* 15 */ B(Wasp::OpCode::DEFINE_LOCAL), B(0),
        /* 17 */ B(Wasp::OpCode::GET_LOCAL), B(0),
        /* 19 */ B(Wasp::OpCode::POP),

        /* 20 */ B(Wasp::OpCode::JUMP), B(11),
        /* 22 */ B(Wasp::OpCode::POP),
        /* 23 */ B(Wasp::OpCode::JUMP), B(25),
        /* 25 */ B(Wasp::OpCode::EXIT_MODULE)};

    EXPECT_EQ(actual_bytes, expected_bytes);
}
