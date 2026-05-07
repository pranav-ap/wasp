add_library(
    ObjectSystemLib
    Objects.cpp
    Objects_oops_object.cpp
    Objects_oops_type.cpp
    ConstantPool.cpp
    ObjectsUtils.cpp
    NativeRegistry.cpp
)

target_link_libraries(ObjectSystemLib PUBLIC BytecodeSystemLib DoctorLib)

target_include_directories(ObjectSystemLib PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR})
