#include "Token.h"
#include "lexer.h"
#include "parser.h"
#include "SemanticAnalyzer.h"
#include "ConstantPool.h"
#include "NativeRegistry.h"
#include "test_utils.h"
#include <gtest/gtest.h>

TEST(Semantics, Simple)
{
    auto mod = parse("25");

    auto pool = std::make_shared<Wasp::ConstantPool>();

    auto native_registry = std::make_shared<Wasp::NativeRegistry>(pool);
    native_registry->load_stdlib();

    auto semantic_analyzer = Wasp::SemanticAnalyzer(native_registry);
    semantic_analyzer.run(mod);
}
