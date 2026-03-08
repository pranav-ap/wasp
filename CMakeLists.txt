cmake_minimum_required(VERSION 3.25)
project(wasp_root)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Enable strict warnings to catch bugs early!
# add_compile_options(-Wall -Wextra -Wpedantic)

# ==========================================
# Build Optimizations
# ==========================================

# Use CCache if installed
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
endif()

# Enable Link Time Optimization (LTO) for Release builds
include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported OUTPUT error)
if(ipo_supported)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
endif()

# ==========================================
# Third-Party Libraries
# ==========================================

# CLI11
add_library(CLI11 INTERFACE)
target_include_directories(CLI11 INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# ==========================================
# Wasp Modules
# ==========================================

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

# ==========================================
# Testing
# ==========================================

if(PROJECT_IS_TOP_LEVEL)
    enable_testing()

    include(FetchContent)
    FetchContent_Declare(
      googletest
      GIT_REPOSITORY https://github.com/google/googletest.git
      GIT_TAG        v1.17.0 
    )
    
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    add_subdirectory(tests)
    # add_subdirectory(benchmarks)
endif()