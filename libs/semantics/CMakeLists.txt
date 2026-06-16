add_library(
    SemanticsLib
    Type.cpp
    Symbol.cpp
    SymbolFactory.cpp
    SymbolScope.cpp
    Hoisting.cpp
    TypeChecker_call.cpp
    TypeChecker_constructor.cpp
    TypeChecker_definitions.cpp
    TypeChecker_expressions.cpp
    TypeChecker_operators.cpp
    TypeChecker_statements.cpp
    TypeChecker_types.cpp
    TypeChecker_variables.cpp
    TypeChecker.cpp
    TypeSystem_assignable.cpp
    TypeSystem_calculate.cpp
    TypeSystem_call_resolution.cpp
    TypeSystem_checks.cpp
    TypeSystem_equality.cpp
    TypeSystem.cpp
    SemanticsAnalyzer.cpp
)

target_link_libraries(SemanticsLib PUBLIC ASTLib TokenLib WorkspaceLib DoctorLib)

target_include_directories(SemanticsLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
