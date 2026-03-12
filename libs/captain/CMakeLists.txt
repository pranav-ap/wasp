add_library(CaptainLib Captain.cpp)

target_link_libraries(CaptainLib 
    PUBLIC 
        DoctorLib
        TokenLib 
        CompilerLib 
        LexerLib 
        ASTLib  
        ParserLib 
        SemanticAnalyzerLib 
        BytecodeSystemLib 
        ObjectSystemLib
        VMLib
)

target_include_directories(CaptainLib PUBLIC .)
