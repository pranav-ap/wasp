add_library(
    ObjectSystemLib
    Objects.cpp
    Objects_oops_object.cpp
    Objects_module.cpp
    Objects_oops_type.cpp
    Objects_mangle.cpp
    ConstantPool.cpp
    Objects_utils.cpp
    NativeRegistry.cpp
)

target_link_libraries(ObjectSystemLib PUBLIC BytecodeSystemLib DoctorLib)

target_include_directories(ObjectSystemLib PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR})
