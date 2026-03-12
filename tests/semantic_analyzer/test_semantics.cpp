#include "ConstantPool.h"
#include "NativeRegistry.h"
#include "SemanticAnalyzer.h"
#include "test_utils.h"

#include <gtest/gtest.h>
#include <memory>

TEST(Semantics, Simple)
{
    auto block = parse("25");

    auto pool = std::make_shared<Wasp::ConstantPool>();
    auto native_registry = std::make_shared<Wasp::NativeRegistry>(pool);

    auto semantic_analyzer = Wasp::SemanticAnalyzer(native_registry);
    semantic_analyzer.run(block);
}
