include(FetchContent)

set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
  google_benchmark
  GIT_REPOSITORY https://github.com/google/benchmark.git
  GIT_TAG v1.8.3
)

FetchContent_MakeAvailable(google_benchmark)

# Define the benchmark executable
add_executable(bench_lexer bench_lexer.cpp)
target_link_libraries(bench_lexer benchmark::benchmark LexerLib)

# 1. Register it as a test so ctest recognizes it
add_test(NAME LexerBenchmark COMMAND bench_lexer)

# 2. Apply the "benchmark" label for easy exclusion
set_tests_properties(LexerBenchmark PROPERTIES LABELS "benchmarks")