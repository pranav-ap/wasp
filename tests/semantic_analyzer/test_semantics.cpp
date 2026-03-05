#include "Token.h"
#include "lexer.h"
#include "parser.h"
#include "SemanticAnalyzer.h"
#include "test_utils.h"
#include <gtest/gtest.h>

TEST(Semantics, Simple)
{
    auto mod = parse("25");
    auto semantic_analyzer = Wasp::SemanticAnalyzer();
    semantic_analyzer.run(mod);
}
