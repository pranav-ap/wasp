add_library(
    SemanticsLib
    Collector_definitions.cpp
    Collector_types.cpp
    Collector.cpp
    Final_call.cpp
    Final_constructor.cpp
    Final_definitions.cpp
    Final_expressions.cpp
    Final_operators.cpp
    Final_statements.cpp
    Final_types.cpp
    Final_variables.cpp
    Final.cpp
    Hoister.cpp
    SemanticsAnalyzer.cpp
    Symbol.cpp
    SymbolFactory.cpp
    SymbolScope.cpp
    Type.cpp
    TypeSystem_assignable.cpp
    TypeSystem_calculate.cpp
    TypeSystem_call_resolution.cpp
    TypeSystem_checks.cpp
    TypeSystem_equality.cpp
    TypeSystem.cpp
)

target_link_libraries(SemanticsLib PUBLIC ASTLib TokenLib WorkspaceLib DoctorLib)

target_include_directories(SemanticsLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
