add_library(CompilerLib Compiler.cpp CompilerScope.cpp)

target_include_directories(CompilerLib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${LLVM_INCLUDE_DIRS}
)

target_link_directories(CompilerLib PUBLIC ${LLVM_LIBRARY_DIRS})

target_link_libraries(CompilerLib PUBLIC
    ${LLVM_LIBS}
    SalterLib
    SemanticsLib
    ASTLib
    TokenLib
    WorkspaceLib
    DoctorLib
)
