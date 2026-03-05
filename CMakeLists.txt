cmake_minimum_required(VERSION 3.25)
project(wasp_root)

# Setup Google Test 

include(FetchContent)

FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.17.0 
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

# CLI11

add_library(CLI11 INTERFACE)
target_include_directories(CLI11 INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Others 

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(libs/workbook)
add_subdirectory(libs/token)
add_subdirectory(libs/lexer)
add_subdirectory(libs/ast)
add_subdirectory(libs/parser)
add_subdirectory(libs/object_system)
add_subdirectory(libs/symbol)
add_subdirectory(libs/type_system)
add_subdirectory(libs/semantic_analyzer)
add_subdirectory(libs/bytecode_system)
add_subdirectory(libs/compiler)
add_subdirectory(libs/vm)
add_subdirectory(src)
add_subdirectory(tests)
# add_subdirectory(benchmarks)

