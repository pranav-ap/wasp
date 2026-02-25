add_library(TypeSystemLib TypeSystem.cpp)

target_link_libraries(TypeSystemLib PUBLIC ObjectSystemLib SymbolLib TokenLib)

target_include_directories(TypeSystemLib PUBLIC .)
