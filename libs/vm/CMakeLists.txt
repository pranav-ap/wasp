add_library(VMLib VM.cpp VM_members.cpp VM_utils.cpp VM_math.cpp)
target_link_libraries(VMLib PUBLIC ObjectSystemLib BytecodeSystemLib SymbolLib DoctorLib WorkspaceLib)
target_include_directories(VMLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
