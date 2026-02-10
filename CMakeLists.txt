cmake_minimum_required(VERSION 3.25)
project(wasp_root)

# Setup Google Test 

include(FetchContent)

FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/heads/main.zip
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

# Others 

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(libs/lexer)
add_subdirectory(src)
add_subdirectory(tests)
add_subdirectory(benchmarks)

