add_library(
    SemanticAnalyzerLib
    SemanticAnalyzer.cpp
    SemanticAnalyzer_types.cpp
    SemanticAnalyzer_expressions.cpp
    SemanticAnalyzer_control_flow.cpp
    SemanticAnalyzer_variables.cpp
    SemanticAnalyzer_definitions.cpp
    SemanticAnalyzer_imports.cpp
    SemanticAnalyzer_utils.cpp
    SymbolScope.cpp
    DependencyCrawler.cpp
    SymbolHoister.cpp
    TypeChecker.cpp
)

target_link_libraries(SemanticAnalyzerLib PUBLIC TokenLib ASTLib  ObjectSystemLib WorkspaceLib DoctorLib)

target_include_directories(SemanticAnalyzerLib PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR})
