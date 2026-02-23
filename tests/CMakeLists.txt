# Define a single test runner
add_executable(unit_tests
    test_utils.cpp
    test_lexer.cpp
    test_parser_literals.cpp
    test_parser_dots.cpp
    test_parser_stars.cpp
    test_parser_access.cpp
    test_parser_definitions.cpp
    test_parser_variable_definitions.cpp
    test_parser_branching.cpp
    test_parser_looping.cpp
    test_parser_statements.cpp
)

# Link all necessary libraries
target_link_libraries(unit_tests
    PRIVATE
        GTest::gtest_main
        ParserLib
        LexerLib
        TokenLib
)

include(GoogleTest)
gtest_discover_tests(unit_tests
    DISCOVERY_MODE PRE_TEST
    PROPERTIES LABELS "unit"
)
