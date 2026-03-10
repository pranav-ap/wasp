#include "Compiler.h"
#include "InstructionPrinter.h"
#include "NativeRegistry.h"
#include "SemanticAnalyzer.h"
#include "test_utils.h"
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <vector>

class CompileVariables : public ::testing::Test {
protected:
  std::string log_dir;
  bool enable_logging = true;

  Wasp::ConstantPool_ptr pool;
  Wasp::CodeObject current_bytecode;
  Wasp::CFGraph current_graph;

  int pool_size;
  int globals_size;

  void SetUp() override {
    const ::testing::TestInfo *const test_info =
        ::testing::UnitTest::GetInstance()->current_test_info();

    std::string suite_name = test_info->test_suite_name();

    log_dir = "/workspaces/wasp/logs/compiler_tests/" + suite_name;

    if (enable_logging && !std::filesystem::exists(log_dir)) {
      std::filesystem::create_directories(log_dir);
    }

    std::string dots_dir = log_dir + "/dots";

    if (enable_logging && !std::filesystem::exists(dots_dir)) {
      std::filesystem::create_directories(dots_dir);
    }
  }

  static std::byte B(Wasp::OpCode op) { return static_cast<std::byte>(op); }
  static std::byte B(int operand) { return static_cast<std::byte>(operand); }

  std::vector<std::byte> compile(const std::string &source) {
    auto mod = parse(source);

    pool = std::make_shared<Wasp::ConstantPool>();

    auto native_registry = std::make_shared<Wasp::NativeRegistry>(pool);
    native_registry->load_stdlib();

    auto semantic_analyzer = Wasp::SemanticAnalyzer(native_registry);
    semantic_analyzer.run(mod);

    Wasp::Compiler compiler(pool);
    current_bytecode = compiler.run(mod);
    current_graph = compiler.get_graph();

    if (enable_logging) {
      log();
    }

    pool_size = pool->get_size();
    globals_size = native_registry->get_size();

    const std::byte *data = current_bytecode.data();
    return std::vector<std::byte>(data, data + current_bytecode.length());
  }

  void log() {
    const ::testing::TestInfo *const test_info =
        ::testing::UnitTest::GetInstance()->current_test_info();
    std::string test_name = test_info->name();

    std::string file_path = log_dir + "/" + test_name + ".txt";
    std::ofstream log_file(file_path);

    std::string dot_file_path = log_dir + "/dots/" + test_name + ".dot";
    std::ofstream dot_file(dot_file_path);

    Wasp::InstructionPrinter printer(pool);

    if (dot_file.is_open()) {
      printer.print(current_graph, dot_file);
      dot_file.close();
    }

    if (log_file.is_open()) {
      printer.print(current_bytecode, log_file);
      printer.print_pool(log_file);
      log_file.close();
    }
  }
};

TEST_F(CompileVariables, DefineAndUseVariable) {
  auto actual_bytes = compile(R"(
let x = 42
x + 1
)");

  int val_42 = pool_size++;
  int val_1 = pool_size++;

  int var_x = globals_size++;

  std::vector<std::byte> expected_bytes = {
      /* 0 */ B(Wasp::OpCode::ENTER_MODULE),
      /* 1 */ B(Wasp::OpCode::LOAD_CONST),   B(val_42), // 42
      /* 3 */ B(Wasp::OpCode::DEFINE_LOCAL), B(var_x),

      /* 5 */ B(Wasp::OpCode::GET_GLOBAL),   B(var_x), // "x"
      /* 7 */ B(Wasp::OpCode::LOAD_CONST),   B(val_1), // 1
      /* 9 */ B(Wasp::OpCode::ADD),
      /* 10*/ B(Wasp::OpCode::POP),
      /* 11*/ B(Wasp::OpCode::JUMP),         B(14),     B(0),
      /* 14*/ B(Wasp::OpCode::EXIT_MODULE)};

  EXPECT_EQ(actual_bytes, expected_bytes);
}


TEST_F(CompileVariables, DefineAndReAssignVariable) {
  auto actual_bytes = compile(R"(
let x = 42
x = x + 1
)");

  int val_42 = pool_size++;
  int val_1 = pool_size++;
  
  int var_x = globals_size++;

  std::vector<std::byte> expected_bytes = {
      B(Wasp::OpCode::ENTER_MODULE),
      B(Wasp::OpCode::LOAD_CONST),   B(val_42),
      B(Wasp::OpCode::DEFINE_LOCAL), B(var_x),
      B(Wasp::OpCode::GET_GLOBAL),   B(var_x),
      B(Wasp::OpCode::LOAD_CONST),   B(val_1),
      B(Wasp::OpCode::ADD),
      B(Wasp::OpCode::SET_GLOBAL),   B(var_x),
      B(Wasp::OpCode::POP),
      B(Wasp::OpCode::JUMP),         B(16), B(0),
      B(Wasp::OpCode::EXIT_MODULE)};

  EXPECT_EQ(actual_bytes, expected_bytes);
}

