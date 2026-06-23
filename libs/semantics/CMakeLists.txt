add_library(
    SemanticsLib
    SemanticsAnalyzer_call.cpp
    SemanticsAnalyzer_collect.cpp
    SemanticsAnalyzer_constructor.cpp
    SemanticsAnalyzer_definitions.cpp
    SemanticsAnalyzer_exports.cpp
    SemanticsAnalyzer_expressions.cpp
    SemanticsAnalyzer_hoist.cpp
    SemanticsAnalyzer_operators.cpp
    SemanticsAnalyzer_resolvers.cpp
    SemanticsAnalyzer_run.cpp
    SemanticsAnalyzer_statements.cpp
    SemanticsAnalyzer_type_nodes.cpp
    SemanticsAnalyzer_utils.cpp
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
