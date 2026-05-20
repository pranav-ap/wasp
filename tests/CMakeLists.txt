set(TEST_COMMON_SRC common/test_utils.cpp)

function(add_wasp_test_suite SUITE_NAME TEST_LABEL)
    add_executable(${SUITE_NAME}
        ${ARGN}
        ${TEST_COMMON_SRC}
    )

    target_include_directories(${SUITE_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/common)

    target_link_libraries(${SUITE_NAME}
        PRIVATE
            GTest::gtest_main
            CompilerLib
            BytecodeSystemLib
            SemanticAnalyzerLib
            ParserLib
            LexerLib
            ASTLib
            TokenLib
    )

    include(GoogleTest)
    gtest_discover_tests(${SUITE_NAME}
        DISCOVERY_MODE PRE_TEST
        PROPERTIES LABELS "unit"
    )
endfunction()


# Lexer Suite
add_wasp_test_suite(lexer_tests "lexer"
    lexer/test_lexer.cpp
)

# Parser Suite
add_wasp_test_suite(parser_tests "parser"
    parser/test_parser_access.cpp
    parser/test_parser_branching.cpp
    parser/test_parser_definitions.cpp
    parser/test_parser_dots.cpp
    parser/test_parser_imports.cpp
    parser/test_parser_literals.cpp
    parser/test_parser_looping.cpp
    parser/test_parser_stars.cpp
    parser/test_parser_statements.cpp
    parser/test_parser_variable_definitions.cpp
)

# Compiler Suite
add_wasp_test_suite(compiler_tests "compiler"
    compiler/test_compile_branches.cpp
    compiler/test_compile_expressions.cpp
    compiler/test_compile_functions.cpp
    compiler/test_compile_loops.cpp
    compiler/test_compile_variables.cpp
)

