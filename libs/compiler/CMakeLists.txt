add_library(
    CompilerLib
    Compiler.cpp
    Compiler_utils.cpp
    Compiler_call.cpp
    Compiler_callable.cpp
    Compiler_constructor.cpp
    Compiler_control_flow.cpp
    Compiler_definitions.cpp
    Compiler_exports.cpp
    Compiler_literals.cpp
    Compiler_oops.cpp
    Compiler_operators.cpp
    Compiler_resolvers.cpp
    Compiler_variables.cpp
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
