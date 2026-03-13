#include "NativeRegistry.h"
#include "SemanticAnalyzer.h"
#include "SymbolHoister.h"
#include "Workspace.h"
#include "test_utils.h"

#include <filesystem>
#include <gtest/gtest.h>
#include <memory>
#include <utility>
#include <vector>

TEST(Semantics, Simple) {
    // 1. Setup Infrastructure
    // We use the current path as a dummy root for the test workspace
    auto workspace = std::make_shared<Wasp::Workspace>(std::filesystem::current_path());

    // 2. Parse the code
    auto block = parse("let x = 25");

    // 3. Wrap in a Module
    // The SemanticAnalyzer now processes Modules to support imports
    auto module = std::make_shared<Wasp::Module>();
    module->file_path = "test.wasp";
    module->block = std::move(block);

    workspace->add_module(module->file_path, module);
    std::vector<Wasp::Module_ptr> build_order = {module};

    // 4. Run Phase 2: Symbol Hoisting
    // This populates the module scope so Phase 3 can do the "Handshake"
    Wasp::SymbolHoister hoister(workspace);
    hoister.run(build_order);

    // 5. Run Phase 3: Semantic Analysis
    // Pass the native_registry from the workspace for consistency
    Wasp::SemanticAnalyzer semantic_analyzer(workspace->native_registry, workspace);

    // This will now successfully link symbols and check types
    semantic_analyzer.run(build_order);
}