add_library(BytecodeSystemLib CFGraph.cpp CodeObjectSerializer.cpp)
target_link_libraries(BytecodeSystemLib PUBLIC ObjectSystemLib DoctorLib)
target_include_directories(BytecodeSystemLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
