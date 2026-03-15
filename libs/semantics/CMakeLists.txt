add_library(
    SemanticAnalyzerLib 
    SemanticAnalyzer.cpp
    SemanticAnalyzerTypes.cpp
    SemanticAnalyzerExpressions.cpp
    TypeChecker.cpp
    SymbolScope.cpp
    DependencyCrawler.cpp
    SymbolHoister.cpp
)

target_link_libraries(SemanticAnalyzerLib PUBLIC TokenLib ASTLib SymbolLib ObjectSystemLib WorkspaceLib DoctorLib)

target_include_directories(SemanticAnalyzerLib PUBLIC .)
