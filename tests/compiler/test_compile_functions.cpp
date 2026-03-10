#include "Compiler.h"
#include "InstructionPrinter.h"
#include "NativeRegistry.h"
#include "SemanticAnalyzer.h"
#include "test_utils.h"
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <vector>

class CompileFunctions : public ::testing::Test {
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

    if (enable_logging) {
      if (!std::filesystem::exists(log_dir)) {
        std::filesystem::create_directories(log_dir);
      }

      // Create 'dots' subdirectory
      std::string dots_dir = log_dir + "/dots";
      if (!std::filesystem::exists(dots_dir)) {
        std::filesystem::create_directories(dots_dir);
      }
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

TEST_F(CompileFunctions, AddFunction) {
  auto actual_bytes = compile(R"(
fun add(a: int, b: int) => int
    return a + b
)");

  int add_func_pool_id = 11;
  int add_func_var_id = globals_size;

  std::vector<std::byte> expected_bytes = {B(Wasp::OpCode::ENTER_MODULE),
                                           B(Wasp::OpCode::LOAD_CONST), B(add_func_pool_id),
                                           B(Wasp::OpCode::MAKE_FUNCTION), B(0),
                                           B(Wasp::OpCode::DEFINE_LOCAL), B(add_func_var_id),
                                           B(Wasp::OpCode::JUMP), B(10), B(0),
                                           B(Wasp::OpCode::EXIT_MODULE)};

  EXPECT_EQ(actual_bytes, expected_bytes);

  auto pool_obj = pool->get(add_func_pool_id);
  ASSERT_TRUE(pool_obj->is<std::shared_ptr<Wasp::FunctionObject>>());
  auto func_obj = pool_obj->as<std::shared_ptr<Wasp::FunctionObject>>();
  const Wasp::CodeObject &inner_code = func_obj->code;

  std::vector<std::byte> actual_inner_bytes(
      inner_code.data(), inner_code.data() + inner_code.length());

  std::vector<std::byte> expected_inner_bytes = {B(Wasp::OpCode::PUSH_SCOPE),
                                                 B(Wasp::OpCode::GET_LOCAL),
                                                 B(0),
                                                 B(Wasp::OpCode::GET_LOCAL),
                                                 B(1),
                                                 B(Wasp::OpCode::ADD),
                                                 B(Wasp::OpCode::RETURN),
                                                 B(Wasp::OpCode::POP_SCOPE),
                                                 B(Wasp::OpCode::LOAD_NONE),
                                                 B(Wasp::OpCode::RETURN)};

  EXPECT_EQ(actual_inner_bytes, expected_inner_bytes);
}

TEST_F(CompileFunctions, MaxFunction) {
  auto actual_bytes = compile(R"(
fun max(a: int, b: int) => int
    if a > b then 
        return a
    else
        return b
)");

  int max_func_pool_id = 11;
  int max_func_var_id = globals_size;

  std::vector<std::byte> expected_bytes = {B(Wasp::OpCode::ENTER_MODULE),
                                           B(Wasp::OpCode::LOAD_CONST),
                                           B(max_func_pool_id),
                                           B(Wasp::OpCode::MAKE_FUNCTION),
                                           B(0),
                                           B(Wasp::OpCode::DEFINE_LOCAL),
                                           B(max_func_var_id),
                                           B(Wasp::OpCode::JUMP),
                                           B(10),
                                           B(0),
                                           B(Wasp::OpCode::EXIT_MODULE)};

  EXPECT_EQ(actual_bytes, expected_bytes);

  auto pool_obj = pool->get(max_func_pool_id);
  ASSERT_TRUE(pool_obj->is<std::shared_ptr<Wasp::FunctionObject>>());
  auto func_obj = pool_obj->as<std::shared_ptr<Wasp::FunctionObject>>();
  const Wasp::CodeObject &inner_code = func_obj->code;

  std::vector<std::byte> actual_inner_bytes(
      inner_code.data(), inner_code.data() + inner_code.length());

  std::vector<std::byte> expected_inner_bytes = {B(Wasp::OpCode::PUSH_SCOPE),
                                                 B(Wasp::OpCode::PUSH_SCOPE),
                                                 B(Wasp::OpCode::GET_LOCAL),
                                                 B(0),
                                                 B(Wasp::OpCode::GET_LOCAL),
                                                 B(1),
                                                 B(Wasp::OpCode::JUMP_IF_FALSE),
                                                 B(22),
                                                 B(0),
                                                 B(Wasp::OpCode::JUMP),
                                                 B(12),
                                                 B(0),
                                                 B(Wasp::OpCode::GET_LOCAL),
                                                 B(0),
                                                 B(Wasp::OpCode::RETURN),
                                                 B(Wasp::OpCode::POP_SCOPE),
                                                 B(Wasp::OpCode::JUMP),
                                                 B(19),
                                                 B(0),
                                                 B(Wasp::OpCode::POP_SCOPE),
                                                 B(Wasp::OpCode::LOAD_NONE),
                                                 B(Wasp::OpCode::RETURN),
                                                 B(Wasp::OpCode::POP_SCOPE),
                                                 B(Wasp::OpCode::PUSH_SCOPE),
                                                 B(Wasp::OpCode::GET_LOCAL),
                                                 B(1),
                                                 B(Wasp::OpCode::RETURN),
                                                 B(Wasp::OpCode::POP_SCOPE),
                                                 B(Wasp::OpCode::JUMP),
                                                 B(19),
                                                 B(0)};

  EXPECT_EQ(actual_inner_bytes, expected_inner_bytes);
}

TEST_F(CompileFunctions, SimpleClosure) {
  auto actual_bytes = compile(R"(
fun outer(a: int) => any
    fun inner() => int
        return a
    return inner
)");

  int inner_func_pool_id = pool_size;
  int outer_func_pool_id = pool_size + 1;
  int outer_func_var_id = globals_size;

  std::vector<std::byte> expected_bytes = {B(Wasp::OpCode::ENTER_MODULE),
                                           B(Wasp::OpCode::LOAD_CONST),
                                           B(outer_func_pool_id),
                                           B(Wasp::OpCode::MAKE_FUNCTION),
                                           B(0),
                                           B(Wasp::OpCode::DEFINE_LOCAL),
                                           B(outer_func_var_id),
                                           B(Wasp::OpCode::JUMP),
                                           B(10),
                                           B(0),
                                           B(Wasp::OpCode::EXIT_MODULE)};

  EXPECT_EQ(actual_bytes, expected_bytes);

  auto outer_pool_obj = pool->get(outer_func_pool_id);
  ASSERT_TRUE(outer_pool_obj->is<std::shared_ptr<Wasp::FunctionObject>>());
  const Wasp::CodeObject &outer_code =
      outer_pool_obj->as<std::shared_ptr<Wasp::FunctionObject>>()->code;

  std::vector<std::byte> actual_outer_bytes(
      outer_code.data(), outer_code.data() + outer_code.length());

  std::vector<std::byte> expected_outer_bytes = {B(Wasp::OpCode::PUSH_SCOPE),
                                                 B(Wasp::OpCode::LOAD_CONST),
                                                 B(inner_func_pool_id),
                                                 B(Wasp::OpCode::MAKE_FUNCTION),
                                                 B(1),
                                                 B(1),
                                                 B(0),
                                                 B(Wasp::OpCode::DEFINE_LOCAL),
                                                 B(1),
                                                 B(Wasp::OpCode::GET_LOCAL),
                                                 B(1),
                                                 B(Wasp::OpCode::RETURN),
                                                 B(Wasp::OpCode::POP_SCOPE),
                                                 B(Wasp::OpCode::LOAD_NONE),
                                                 B(Wasp::OpCode::RETURN)};

  EXPECT_EQ(actual_outer_bytes, expected_outer_bytes);
}

TEST_F(CompileFunctions, Print) {
  auto actual_bytes = compile(R"(
print(1)
)");

  int print_func_var_id = 0; 
  int const_one_id = pool_size;

  std::vector<std::byte> expected_bytes = {B(Wasp::OpCode::ENTER_MODULE),
                                           B(Wasp::OpCode::GET_GLOBAL),
                                           B(print_func_var_id),
                                           B(Wasp::OpCode::LOAD_CONST),
                                           B(const_one_id),
                                           B(Wasp::OpCode::CALL),
                                           B(1),
                                           B(Wasp::OpCode::POP),
                                           B(Wasp::OpCode::JUMP),
                                           B(11),
                                           B(0),
                                           B(Wasp::OpCode::EXIT_MODULE)};

  EXPECT_EQ(actual_bytes, expected_bytes);
}
