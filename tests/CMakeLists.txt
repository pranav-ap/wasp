add_executable(lexer_tests test_lexer.cpp)

target_link_libraries(lexer_tests 
    GTest::gtest_main 
    LexerLib
)

include(GoogleTest)
gtest_discover_tests(lexer_tests)
