add_library(
    ParserLib 
    parser.cpp 
    ExpressionParselets.cpp 
    TokenPipe.cpp 
    register_parselets.cpp
)

target_link_libraries(ParserLib PUBLIC ASTLib TokenLib)

target_include_directories(ParserLib PUBLIC .)
