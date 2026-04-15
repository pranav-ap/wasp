#include "SemanticAnalyzer.h"
#include "Workspace.h"
#include "test_utils.h"

#include <filesystem>
#include <gtest/gtest.h>
#include <memory>
#include <vector>

TEST(Semantics, Simple) {
    auto workspace = std::make_shared<Wasp::Workspace>(std::filesystem::current_path());

    auto stmts = parse("let x = 25");

    auto module = std::make_shared<Wasp::Module>("test.wasp", stmts);

    workspace->add_module(module->absolute_filepath, module);
    std::vector<Wasp::Module_ptr> build_order = {module};

    Wasp::SemanticAnalyzer semantic_analyzer(workspace);

    semantic_analyzer.run(build_order);
}
