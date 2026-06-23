add_library(CompilerLib Compiler.cpp)

target_include_directories(CompilerLib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(CompilerLib PUBLIC
    SalterLib
    SemanticsLib
    ASTLib
    TokenLib
    WorkspaceLib
    DoctorLib
)
