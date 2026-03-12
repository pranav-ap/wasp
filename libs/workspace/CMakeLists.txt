add_library(WorkspaceLib INTERFACE)

target_link_libraries(WorkspaceLib INTERFACE ObjectSystemLib BytecodeSystemLib SymbolLib ASTLib)

target_include_directories(WorkspaceLib INTERFACE .)
