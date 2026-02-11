add_library(LexerLib lexer.cpp)

target_link_libraries(LexerLib PUBLIC TokenLib)

target_include_directories(LexerLib PUBLIC .)
