add_library(CaptainLib Captain.cpp CaptainUtils.cpp DependencyCrawler.cpp)

target_link_libraries(CaptainLib
    PUBLIC
        DoctorLib
        TokenLib
        WorkspaceLib
        LexerLib
        ASTLib
        ParserLib
        SemanticsLib
        CompilerLib
)

target_include_directories(CaptainLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
