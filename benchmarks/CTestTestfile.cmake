# CMake generated Testfile for 
# Source directory: /workspaces/wasp/benchmarks
# Build directory: /workspaces/wasp/benchmarks
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test([=[LexerBenchmark]=] "/workspaces/wasp/benchmarks/bench_lexer")
set_tests_properties([=[LexerBenchmark]=] PROPERTIES  LABELS "benchmarks" _BACKTRACE_TRIPLES "/workspaces/wasp/benchmarks/CMakeLists.txt;18;add_test;/workspaces/wasp/benchmarks/CMakeLists.txt;0;")
subdirs("../_deps/google_benchmark-build")
