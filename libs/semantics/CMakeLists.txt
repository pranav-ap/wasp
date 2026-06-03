add_library(
    SemanticAnalyzerLib
    SemanticAnalyzer.cpp
    SemanticAnalyzer_variables.cpp
    SemanticAnalyzer_callables.cpp
    SemanticAnalyzer_oops.cpp
    SemanticAnalyzer_exports.cpp
    SemanticAnalyzer_types.cpp
    SemanticAnalyzer_hoist.cpp
    SemanticAnalyzer_constructor.cpp
    SemanticAnalyzer_resolvers.cpp
    SemanticAnalyzer_enum_resolver.cpp
    SemanticAnalyzer_call.cpp
    SemanticAnalyzer_call_templates.cpp
    SemanticAnalyzer_control_flow.cpp
    SemanticAnalyzer_literals.cpp
    SemanticAnalyzer_definitions.cpp
    SemanticAnalyzer_operators.cpp
    SemanticAnalyzer_utils.cpp
    DependencyCrawler.cpp
    SymbolScope.cpp
    TypeSystem_overloads.cpp
    TypeSystem_infer.cpp
    TypeSystem_templates.cpp
    TypeSystem_utils.cpp
    ASTCloner.cpp
    ASTFactory.cpp
    Salt.cpp
)

target_link_libraries(SemanticAnalyzerLib PUBLIC TokenLib ASTLib ObjectSystemLib WorkspaceLib DoctorLib)

target_include_directories(SemanticAnalyzerLib PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR})
