include(FetchContent)

FetchContent_Declare(
  google_benchmark
  GIT_REPOSITORY https://github.com/google/benchmark.git
  GIT_TAG v1.8.3
)

FetchContent_MakeAvailable(google_benchmark)

add_executable(bench_lexer bench_lexer.cpp)
target_link_libraries(bench_lexer benchmark::benchmark LexerLib)
