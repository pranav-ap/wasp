add_library(
    CompilerLib
    Compiler.cpp
    Compiler_imports.cpp
    Compiler_control_flow.cpp
    Compiler_expressions.cpp
    Compiler_expressions_boring.cpp
    Compiler_variables.cpp
    Compiler_definitions.cpp
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
