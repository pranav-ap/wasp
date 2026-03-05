add_library(VMLib VM.cpp VMMath.cpp)
target_link_libraries(VMLib PUBLIC ObjectSystemLib BytecodeSystemLib SymbolLib)
target_include_directories(VMLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
