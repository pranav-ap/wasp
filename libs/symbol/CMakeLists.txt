add_library(SymbolLib SymbolScope.cpp)

target_link_libraries(SymbolLib PUBLIC ObjectSystemLib)

target_include_directories(SymbolLib PUBLIC .)
