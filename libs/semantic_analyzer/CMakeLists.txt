add_library(SemanticAnalyzerLib SemanticAnalyzer.cpp SemanticAnalyzerTypes.cpp TypeSystem.cpp SymbolScope.cpp)

target_link_libraries(SemanticAnalyzerLib PUBLIC TokenLib ASTLib SymbolLib ObjectSystemLib DoctorLib)

target_include_directories(SemanticAnalyzerLib PUBLIC .)
