add_library(SemanticAnalyzerLib SemanticAnalyzer.cpp SemanticAnalyzerExpression.cpp SemanticAnalyzerStatement.cpp SemanticAnalyzerType.cpp SemanticAnalyzerUtils.cpp)

target_link_libraries(SemanticAnalyzerLib PUBLIC TokenLib ASTLib SymbolLib TypeSystemLib)

target_include_directories(SemanticAnalyzerLib PUBLIC .)
