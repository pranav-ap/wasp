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

        current_name_map = compiler.get_name_map();
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

        Wasp::InstructionPrinter printer(current_pool, current_name_map);

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
        /* 0 */ B(Wasp::OpCode::ENTER_MODULE),
        /* 1 */ B(Wasp::OpCode::LOAD_CONST), B(10),
        /* 3 */ B(Wasp::OpCode::POP),

        /* 4 */ B(Wasp::OpCode::JUMP), B(7), B(0),

        /* 7 */ B(Wasp::OpCode::EXIT_MODULE)};

    EXPECT_EQ(actual_bytes, expected_bytes);
}

TEST_F(CompilerTest, SimpleList)
{
    auto actual_bytes = compile("[1, 2, 3]");

    std::vector<std::byte> expected_bytes = {
        /* 0 */ B(Wasp::OpCode::ENTER_MODULE),
        /* 1 */ B(Wasp::OpCode::LOAD_CONST), B(10),
        /* 3 */ B(Wasp::OpCode::LOAD_CONST), B(11),
        /* 5 */ B(Wasp::OpCode::LOAD_CONST), B(12),
        /* 7 */ B(Wasp::OpCode::BUILD_LIST), B(3),
        /* 9 */ B(Wasp::OpCode::POP),

        /* 10*/ B(Wasp::OpCode::JUMP), B(13), B(0),

        /* 13*/ B(Wasp::OpCode::EXIT_MODULE)};

    EXPECT_EQ(actual_bytes, expected_bytes);
}

TEST_F(CompilerTest, NegateNumber)
{
    auto actual_bytes = compile("-2");

    std::vector<std::byte> expected_bytes = {
        /* 0 */ B(Wasp::OpCode::ENTER_MODULE),
        /* 1 */ B(Wasp::OpCode::LOAD_CONST), B(10),
        /* 3 */ B(Wasp::OpCode::NEGATE),
        /* 4 */ B(Wasp::OpCode::POP),

        /* 5 */ B(Wasp::OpCode::JUMP), B(8), B(0),

        /* 8 */ B(Wasp::OpCode::EXIT_MODULE)};

    EXPECT_EQ(actual_bytes, expected_bytes);
}

TEST_F(CompilerTest, SimpleAddition)
{
    auto actual_bytes = compile("1 + 2");

    std::vector<std::byte> expected_bytes = {
        /* 0 */ B(Wasp::OpCode::ENTER_MODULE),
        /* 1 */ B(Wasp::OpCode::LOAD_CONST), B(10),
        /* 3 */ B(Wasp::OpCode::LOAD_CONST), B(11),
        /* 5 */ B(Wasp::OpCode::ADD),
        /* 6 */ B(Wasp::OpCode::POP),

        /* 7 */ B(Wasp::OpCode::JUMP), B(10), B(0),

        /* 10*/ B(Wasp::OpCode::EXIT_MODULE)};

    EXPECT_EQ(actual_bytes, expected_bytes);
}

TEST_F(CompilerTest, DefineAndUseVariable)
{
    auto actual_bytes = compile(R"(
let x = 42
x + 1
)");

    std::vector<std::byte> expected_bytes = {
        /* 0 */ B(Wasp::OpCode::ENTER_MODULE),

        /* 1 */ B(Wasp::OpCode::LOAD_CONST), B(10),
        /* 3 */ B(Wasp::OpCode::DEFINE_LOCAL), B(0),

        /* 5 */ B(Wasp::OpCode::GET_LOCAL), B(0),
        /* 7 */ B(Wasp::OpCode::LOAD_CONST), B(11),
        /* 9 */ B(Wasp::OpCode::ADD),
        /* 10*/ B(Wasp::OpCode::POP),

        /* 11*/ B(Wasp::OpCode::JUMP), B(14), B(0),

        /* 14*/ B(Wasp::OpCode::EXIT_MODULE)};

    EXPECT_EQ(actual_bytes, expected_bytes);
}

TEST_F(CompilerTest, DefineAndReAssignVariable)
{
    auto actual_bytes = compile(R"(
let x = 42
x = x + 1
)");

    std::vector<std::byte> expected_bytes = {
        /* 0 */ B(Wasp::OpCode::ENTER_MODULE),

        /* 1 */ B(Wasp::OpCode::LOAD_CONST), B(10),
        /* 3 */ B(Wasp::OpCode::DEFINE_LOCAL), B(0),

        /* 5 */ B(Wasp::OpCode::GET_LOCAL), B(0),
        /* 7 */ B(Wasp::OpCode::LOAD_CONST), B(11),
        /* 9 */ B(Wasp::OpCode::ADD),
        /* 10*/ B(Wasp::OpCode::SET_LOCAL), B(0),
        /* 12*/ B(Wasp::OpCode::POP),

        /* 13*/ B(Wasp::OpCode::JUMP), B(16), B(0),

        /* 16*/ B(Wasp::OpCode::EXIT_MODULE)};

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
        /* 2 */ B(Wasp::OpCode::JUMP_IF_FALSE), B(16), B(0),
        /* 5 */ B(Wasp::OpCode::JUMP), B(8), B(0),

        // True Block
        /* 8 */ B(Wasp::OpCode::PUSH_SCOPE),
        /* 9 */ B(Wasp::OpCode::LOAD_CONST), B(10),
        /* 11*/ B(Wasp::OpCode::POP),
        /* 12*/ B(Wasp::OpCode::POP_SCOPE),
        /* 13*/ B(Wasp::OpCode::JUMP), B(16), B(0),

        // End Block
        /* 16*/ B(Wasp::OpCode::JUMP), B(19), B(0),

        /* 19*/ B(Wasp::OpCode::EXIT_MODULE)};

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
        /* 2 */ B(Wasp::OpCode::JUMP_IF_FALSE), B(19), B(0),
        /* 5 */ B(Wasp::OpCode::JUMP), B(8), B(0),

        // True Block
        /* 8 */ B(Wasp::OpCode::PUSH_SCOPE),
        /* 9 */ B(Wasp::OpCode::LOAD_CONST), B(10),
        /* 11*/ B(Wasp::OpCode::POP),
        /* 12*/ B(Wasp::OpCode::POP_SCOPE),
        /* 13*/ B(Wasp::OpCode::JUMP), B(16), B(0),

        // End Block
        /* 16*/ B(Wasp::OpCode::JUMP), B(27), B(0),

        // False Block (Else)
        /* 19*/ B(Wasp::OpCode::PUSH_SCOPE),
        /* 20*/ B(Wasp::OpCode::LOAD_CONST), B(10),
        /* 22*/ B(Wasp::OpCode::POP),
        /* 23*/ B(Wasp::OpCode::POP_SCOPE),
        /* 24*/ B(Wasp::OpCode::JUMP), B(16), B(0),

        /* 27*/ B(Wasp::OpCode::EXIT_MODULE)};

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
        /* 2 */ B(Wasp::OpCode::JUMP_IF_FALSE), B(19), B(0),
        /* 5 */ B(Wasp::OpCode::JUMP), B(8), B(0),

        // If True Block
        /* 8 */ B(Wasp::OpCode::PUSH_SCOPE),
        /* 9 */ B(Wasp::OpCode::LOAD_CONST), B(10),
        /* 11 */ B(Wasp::OpCode::POP),
        /* 12 */ B(Wasp::OpCode::POP_SCOPE),
        /* 13 */ B(Wasp::OpCode::JUMP), B(16), B(0),
        /* 16 */ B(Wasp::OpCode::JUMP), B(45), B(0),

        // Elif Condition
        /* 19 */ B(Wasp::OpCode::LOAD_TRUE),
        /* 20 */ B(Wasp::OpCode::JUMP_IF_FALSE), B(37), B(0),
        /* 23 */ B(Wasp::OpCode::JUMP), B(26), B(0),

        // Elif True Block
        /* 26 */ B(Wasp::OpCode::PUSH_SCOPE),
        /* 27 */ B(Wasp::OpCode::LOAD_CONST), B(10),
        /* 29 */ B(Wasp::OpCode::POP),
        /* 30 */ B(Wasp::OpCode::POP_SCOPE),
        /* 31 */ B(Wasp::OpCode::JUMP), B(34), B(0),
        /* 34 */ B(Wasp::OpCode::JUMP), B(16), B(0),

        // Else Block
        /* 37 */ B(Wasp::OpCode::PUSH_SCOPE),
        /* 38 */ B(Wasp::OpCode::LOAD_CONST), B(10),
        /* 40 */ B(Wasp::OpCode::POP),
        /* 41 */ B(Wasp::OpCode::POP_SCOPE),
        /* 42 */ B(Wasp::OpCode::JUMP), B(34), B(0),

        /* 45 */ B(Wasp::OpCode::EXIT_MODULE)};

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
        /* 2 */ B(Wasp::OpCode::JUMP_IF_FALSE), B(10), B(0),

        // True Expression
        /* 5 */ B(Wasp::OpCode::LOAD_CONST), B(10),
        /* 7 */ B(Wasp::OpCode::JUMP), B(15), B(0),

        // False Expression
        /* 10*/ B(Wasp::OpCode::LOAD_CONST), B(11),
        /* 12*/ B(Wasp::OpCode::JUMP), B(15), B(0),

        // End of Ternary
        /* 15*/ B(Wasp::OpCode::POP),

        /* 16*/ B(Wasp::OpCode::JUMP), B(19), B(0),
        /* 19*/ B(Wasp::OpCode::EXIT_MODULE)};

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
        /* 2 */ B(Wasp::OpCode::DUP),
        /* 3 */ B(Wasp::OpCode::DEFINE_LOCAL), B(0),

        /* 5 */ B(Wasp::OpCode::JUMP_IF_FALSE), B(19), B(0),
        /* 8 */ B(Wasp::OpCode::JUMP), B(11), B(0),

        // True Block
        /* 11*/ B(Wasp::OpCode::PUSH_SCOPE),
        /* 12*/ B(Wasp::OpCode::LOAD_CONST), B(10),
        /* 14*/ B(Wasp::OpCode::POP),
        /* 15*/ B(Wasp::OpCode::POP_SCOPE),
        /* 16*/ B(Wasp::OpCode::JUMP), B(19), B(0),
        /* 19*/ B(Wasp::OpCode::JUMP), B(22), B(0),

        /* 22*/ B(Wasp::OpCode::EXIT_MODULE)};

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

        /* 1 */ B(Wasp::OpCode::JUMP), B(4), B(0),

        // Header
        /* 4 */ B(Wasp::OpCode::LOAD_TRUE),
        /* 5 */ B(Wasp::OpCode::JUMP_IF_FALSE), B(19), B(0),
        /* 8 */ B(Wasp::OpCode::JUMP), B(11), B(0),

        // Body
        /* 11*/ B(Wasp::OpCode::PUSH_SCOPE),
        /* 12*/ B(Wasp::OpCode::LOAD_CONST), B(10),
        /* 14*/ B(Wasp::OpCode::POP),
        /* 15*/ B(Wasp::OpCode::POP_SCOPE),

        /* 16*/ B(Wasp::OpCode::JUMP), B(4), B(0),

        // End block
        /* 19*/ B(Wasp::OpCode::JUMP), B(22), B(0),
        /* 22*/ B(Wasp::OpCode::EXIT_MODULE)};

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

        /* 1 */ B(Wasp::OpCode::JUMP), B(4), B(0),

        // Header
        /* 4 */ B(Wasp::OpCode::LOAD_TRUE),
        /* 5 */ B(Wasp::OpCode::NOT),
        /* 6 */ B(Wasp::OpCode::JUMP_IF_FALSE), B(20), B(0),
        /* 9 */ B(Wasp::OpCode::JUMP), B(12), B(0),

        // Body
        /* 12*/ B(Wasp::OpCode::PUSH_SCOPE),
        /* 13*/ B(Wasp::OpCode::LOAD_CONST), B(10),
        /* 15*/ B(Wasp::OpCode::POP),
        /* 16*/ B(Wasp::OpCode::POP_SCOPE),

        /* 17*/ B(Wasp::OpCode::JUMP), B(4), B(0),

        // End Block
        /* 20*/ B(Wasp::OpCode::JUMP), B(23), B(0),
        /* 23*/ B(Wasp::OpCode::EXIT_MODULE)};

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

        /* 1 */ B(Wasp::OpCode::LOAD_CONST), B(10),
        /* 3 */ B(Wasp::OpCode::LOAD_CONST), B(11),
        /* 5 */ B(Wasp::OpCode::LOAD_CONST), B(12),
        /* 7 */ B(Wasp::OpCode::BUILD_LIST), B(3),

        /* 9 */ B(Wasp::OpCode::JUMP), B(12), B(0),

        // Header
        /* 12*/ B(Wasp::OpCode::LOOP_ITER), B(28), B(0),
        /* 15*/ B(Wasp::OpCode::JUMP), B(18), B(0),

        // Body
        /* 18*/ B(Wasp::OpCode::PUSH_SCOPE),
        /* 19*/ B(Wasp::OpCode::DEFINE_LOCAL), B(0),
        /* 21*/ B(Wasp::OpCode::GET_LOCAL), B(0),
        /* 23*/ B(Wasp::OpCode::POP),
        /* 24*/ B(Wasp::OpCode::POP_SCOPE),

        /* 25*/ B(Wasp::OpCode::JUMP), B(12), B(0),

        /* 28*/ B(Wasp::OpCode::POP),

        /* 29*/ B(Wasp::OpCode::JUMP), B(32), B(0),
        /* 32*/ B(Wasp::OpCode::EXIT_MODULE)};

    EXPECT_EQ(actual_bytes, expected_bytes);
}
