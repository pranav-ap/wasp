project(wasp)

add_executable(wasp main.cpp)

target_link_libraries(wasp
    PRIVATE
        CLI11
        LexerLib
        ParserLib
        SemanticAnalyzerLib
        CompilerLib 
        VMLib
)
