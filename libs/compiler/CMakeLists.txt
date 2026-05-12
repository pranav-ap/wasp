add_library(
    CompilerLib
    Compiler.cpp
    Compiler_utils.cpp
    InstructionPrinter.cpp
)

target_link_libraries(
    CompilerLib
    PUBLIC
    WorkspaceLib
    ObjectSystemLib
    BytecodeSystemLib

    ASTLib
    TokenLib
    DoctorLib
)

target_include_directories(
    CompilerLib
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
