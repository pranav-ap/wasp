add_library(
    SemanticsLib
    Collector_definitions.cpp
    Collector_types.cpp
    Collector.cpp
    Terminator_call.cpp
    Terminator_constructor.cpp
    Terminator_definitions.cpp
    Terminator_expressions.cpp
    Terminator_operators.cpp
    Terminator_statements.cpp
    Terminator_types.cpp
    Terminator_variables.cpp
    Hoister.cpp
    Phase.cpp
    SemanticsAnalyzer.cpp
    Symbol.cpp
    SymbolFactory.cpp
    SymbolScope.cpp
    Type.cpp
    TypeSystem_assignable.cpp
    TypeSystem_calculate.cpp
    TypeSystem_calls.cpp
    TypeSystem_checks.cpp
    TypeSystem_equality.cpp
    TypeSystem_operators.cpp
    TypeSystem_traits.cpp
    TypeSystem_utils.cpp
)

target_link_libraries(SemanticsLib PUBLIC ASTLib TokenLib WorkspaceLib DoctorLib)

target_include_directories(SemanticsLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
