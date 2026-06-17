add_library(
    CompilerLib
    Compiler.cpp
)

target_link_libraries(CompilerLib PUBLIC  ${LLVM_LIBS} ASTLib TokenLib WorkspaceLib DoctorLib)

target_include_directories(CompilerLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
