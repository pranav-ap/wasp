#include <benchmark/benchmark.h>
#include "lexer.h"

static void BM_LexerExample(benchmark::State& state) {
    std::string input = "some long string to lex...";
    
    for (auto _ : state) {
        // auto result = Lexer::lex(input);
        // benchmark::DoNotOptimize(result); // Prevents compiler from optimizing out the call
    }
}

// Register the function as a benchmark
BENCHMARK(BM_LexerExample);

// Standard main for benchmarks
BENCHMARK_MAIN();
