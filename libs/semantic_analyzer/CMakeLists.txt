add_library(SemanticAnalyzerLib SemanticAnalyzer.cpp SemanticAnalyzerTypes.cpp)

target_link_libraries(SemanticAnalyzerLib PUBLIC TokenLib ASTLib SymbolLib TypeSystemLib)

target_include_directories(SemanticAnalyzerLib PUBLIC .)
