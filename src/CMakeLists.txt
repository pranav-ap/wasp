project(wasp)

add_executable(wasp main.cpp)

target_link_libraries(wasp
    PRIVATE
        CLI11
        DoctorLib
        LexerLib
        ParserLib
        SemanticAnalyzerLib
        CompilerLib 
        CaptainLib
        VMLib
)
