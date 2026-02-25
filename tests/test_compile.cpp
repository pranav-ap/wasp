#include "Token.h"
#include "lexer.h"
#include "parser.h"
#include "SemanticAnalyzer.h"
#include "InstructionPrinter.h"
#include "Compiler.h"
#include "test_utils.h"
#include <gtest/gtest.h>
#include <fstream>

TEST(Compile, Simple)
{
    auto mod = parse("25");
    auto semantic_analyzer = Wasp::SemanticAnalyzer();
    semantic_analyzer.run(mod);

    auto compiler = Wasp::Compiler();
    auto [constant_pool, bytecode] = compiler.run(mod);

    auto debug_name_map = compiler.get_name_map();
    std::ofstream log_file("compiler_simple.wasp");
    Wasp::InstructionPrinter printer(constant_pool, debug_name_map);

    if (log_file.is_open())
    {
        printer.print(bytecode, log_file);
        log_file.close();
    }
}
