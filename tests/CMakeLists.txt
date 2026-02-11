add_executable(lexer_tests test_lexer.cpp)
target_link_libraries(lexer_tests GTest::gtest_main LexerLib TokenLib)

include(GoogleTest)
# The DISCOVERY_MODE PRE_TEST is often more robust in container environments like VS Code
gtest_discover_tests(lexer_tests
    DISCOVERY_MODE PRE_TEST
    PROPERTIES LABELS "unit"
)