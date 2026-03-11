add_library(BytecodeSystemLib CFGraph.cpp CodeObjectSerializer.cpp)
target_link_libraries(BytecodeSystemLib PUBLIC ObjectSystemLib)
target_include_directories(BytecodeSystemLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
