#pragma once

#include "CFGraph.h"
#include "Compiler.h"
#include "ConstantPool.h"
#include "InstructionPrinter.h"
#include "NativeRegistry.h"
#include "Objects.h"
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
    Wasp::StaticFunctionObject_ptr function_object;
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
        workspace = std::make_shared<Wasp::Workspace>(std::filesystem::current_path());

        pool = workspace->pool;
        pool_size = pool->get_size();

        auto stmts = parse(source);

        auto module = std::make_shared<Wasp::Module>("test_module.wasp", stmts);

        workspace->add_module(module->absolute_filepath, module);
        std::vector<Wasp::Module_ptr> build_order = {module};

        Wasp::SymbolHoister hoister(workspace);

        for (const auto& mod : build_order)
        {
            hoister.run(mod);
        }

        Wasp::SemanticAnalyzer semantic_analyzer(workspace);
        semantic_analyzer.run(build_order);

        Wasp::Compiler compiler(workspace);

        function_object = compiler.run(module->stmts, "<test>");
        current_graph = compiler.get_graph();

        if (enable_logging) {
            log();
        }

        const std::byte* data = function_object->code.data();
        return std::vector<std::byte>(data, data + function_object->code.length());
    }

    void log() {
        const ::testing::TestInfo* const test_info =
            ::testing::UnitTest::GetInstance()->current_test_info();
        std::string test_name = test_info->name();

        std::string file_path = log_dir + "/" + test_name + ".txt";
        std::ofstream log_file(file_path);

        Wasp::InstructionPrinter printer(workspace);

        if (log_file.is_open()) {
            printer.print(function_object, log_file);
            printer.print_pool_functions(log_file);
        }
    }
};
