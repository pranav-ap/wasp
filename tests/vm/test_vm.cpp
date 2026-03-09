#include "Compiler.h"
#include "InstructionPrinter.h"
#include "NativeRegistry.h"
#include "SemanticAnalyzer.h"
#include "VM.h"

#include "test_utils.h"
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <vector>

class VMTest : public ::testing::Test {
protected:
  std::string log_dir;
  bool enable_logging = true;

  Wasp::ConstantPool_ptr pool;
  Wasp::CodeObject current_bytecode;
  Wasp::CFGraph current_graph;

  void SetUp() override {
    const ::testing::TestInfo *const test_info =
        ::testing::UnitTest::GetInstance()->current_test_info();

    std::string suite_name = test_info->test_suite_name();

    log_dir = "/workspaces/wasp/logs/vm_tests/" + suite_name;

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

    auto main_module =
        std::make_shared<Wasp::FunctionObject>(std::move(current_bytecode));

    Wasp::VM vm(pool, native_registry);
    vm.run(main_module);

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

// ============================================================================
// Control Flow: Branches
// ============================================================================

TEST_F(VMTest, Print) {
  auto actual_bytes = compile(R"(
print(1)
)");

  EXPECT_TRUE(true);
}
