add_library(LexerLib Lexer.cpp)

target_link_libraries(LexerLib PUBLIC TokenLib)

target_include_directories(LexerLib PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR})
