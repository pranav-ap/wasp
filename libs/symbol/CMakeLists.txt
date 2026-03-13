add_library(SymbolLib Symbol.cpp)

target_link_libraries(SymbolLib PUBLIC ObjectSystemLib)

target_include_directories(SymbolLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
