add_library(SymbolLib INTERFACE)

target_link_libraries(SymbolLib INTERFACE ObjectSystemLib)

target_include_directories(SymbolLib INTERFACE .)
