add_library(
    SemanticAnalyzerLib
    SemanticAnalyzer.cpp
    SemanticAnalyzer_variables.cpp
    SemanticAnalyzer_callables.cpp
    SemanticAnalyzer_oops.cpp
    SemanticAnalyzer_exports.cpp
    SemanticAnalyzer_types.cpp
    SemanticAnalyzer_hoist.cpp
    SemanticAnalyzer_ast_transforms.cpp
    SemanticAnalyzer_expressions.cpp
    SemanticAnalyzer_control_flow.cpp
    SemanticAnalyzer_definitions.cpp
    SemanticAnalyzer_utils.cpp
    SymbolScope.cpp
    DependencyCrawler.cpp
    TypeSystem_resolve.cpp
    TypeSystem_infer.cpp
    TypeSystem_boring.cpp
)

target_link_libraries(SemanticAnalyzerLib PUBLIC TokenLib ASTLib  ObjectSystemLib WorkspaceLib DoctorLib)

target_include_directories(SemanticAnalyzerLib PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR})
