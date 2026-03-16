#pragma once

#include "CFGraph.h"
#include "Compiler.h"
#include "ConstantPool.h"
#include "InstructionPrinter.h"
#include "NativeRegistry.h"
#include "OpCode.h"
#include "SemanticAnalyzer.h"
#include "SymbolHoister.h"
#include "Workspace.h"
#include "test_utils.h"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>

class CompilerTestBase : public ::testing::Test {
protected:
    std::string log_dir;
    bool enable_logging = true;

    std::shared_ptr<Wasp::Workspace> workspace;
    Wasp::ConstantPool_ptr pool;
    Wasp::CodeObject current_bytecode;
    Wasp::CFGraph current_graph;

    int pool_size = 0;

    void SetUp() override {
        const ::testing::TestInfo* const test_info =
            ::testing::UnitTest::GetInstance()->current_test_info();

        std::string suite_name = test_info->test_suite_name();

        log_dir = "/workspaces/wasp/logs/compiler_tests/" + suite_name;

        if (enable_logging) {
            if (!std::filesystem::exists(log_dir)) {
                std::filesystem::create_directories(log_dir);
            }
            std::string dots_dir = log_dir + "/dots";
            if (!std::filesystem::exists(dots_dir)) {
                std::filesystem::create_directories(dots_dir);
            }
        }
    }

    static std::byte B(Wasp::OpCode op) { return static_cast<std::byte>(op); }

    static std::byte B(int operand) { return static_cast<std::byte>(operand); }

    std::vector<std::byte> compile(const std::string& source) {
        // 1. Setup the Workspace just like Captain does
        // Using a dummy path for the in-memory test workspace
        workspace = std::make_shared<Wasp::Workspace>(std::filesystem::current_path());

        // Grab the pool from the workspace so the test assertions still work
        pool = workspace->pool;
        pool_size = pool->get_size();

        // 2. Parse the code
        auto block = parse(source);

        // 3. Wrap it in a Mock Module
        auto module = std::make_shared<Wasp::Module>();
        module->file_path = "test_module.wasp";
        module->block = std::move(block);

        workspace->add_module(module->file_path, module);
        std::vector<Wasp::Module_ptr> build_order = {module};

        // 4. Phase 2: Hoist Symbols (Crucial for the Semantic Analyzer Handshake!)
        Wasp::SymbolHoister hoister(workspace);
        hoister.run(build_order);

        // 5. Phase 3: Semantic Analysis
        Wasp::SemanticAnalyzer semantic_analyzer(workspace);
        semantic_analyzer.run(build_order);

        // 6. Phase 4: Bytecode Compilation
        Wasp::Compiler compiler(pool, workspace->native_registry);

        // Note: Depending on how your AST handles moves, you might need to compile module->block
        // instead of the raw block if std::move(block) emptied the original pointer.
        current_bytecode = compiler.run(module->block);
        current_graph = compiler.get_graph();

        if (enable_logging) {
            log();
        }

        const std::byte* data = current_bytecode.data();
        return std::vector<std::byte>(data, data + current_bytecode.length());
    }

    void log() {
        const ::testing::TestInfo* const test_info =
            ::testing::UnitTest::GetInstance()->current_test_info();
        std::string test_name = test_info->name();

        std::string file_path = log_dir + "/" + test_name + ".txt";
        std::ofstream log_file(file_path);

        std::string dot_file_path = log_dir + "/dots/" + test_name + ".dot";
        std::ofstream dot_file(dot_file_path);

        Wasp::InstructionPrinter printer(pool);

        if (dot_file.is_open()) {
            printer.print(current_graph, dot_file);
        }

        if (log_file.is_open()) {
            printer.print(current_bytecode, log_file);
            printer.print_pool(log_file);
        }
    }
};