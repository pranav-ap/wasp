add_library(
    VMLib
    VM.cpp
    VM_utils.cpp
    VM_math.cpp
    VM_execute.cpp
    VM_exports.cpp
    VM_basics.cpp
    )

target_link_libraries(VMLib PUBLIC ObjectSystemLib BytecodeSystemLib DoctorLib WorkspaceLib)

target_include_directories(VMLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
