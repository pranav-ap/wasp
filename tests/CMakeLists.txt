# Define a single test runner
add_executable(unit_tests
    test_lexer.cpp
    test_parser.cpp
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
