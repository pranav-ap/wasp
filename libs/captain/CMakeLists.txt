add_library(CaptainLib Captain.cpp CaptainUtils.cpp)

target_link_libraries(CaptainLib 
    PUBLIC 
        DoctorLib
        TokenLib 
        WorkspaceLib
        CompilerLib 
        LexerLib 
        ASTLib  
        ParserLib 
        SemanticAnalyzerLib 
        BytecodeSystemLib 
        ObjectSystemLib
        VMLib
)

target_include_directories(CaptainLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
