add_library(
    ObjectSystemLib
    Objects.cpp
    RuntimeObjects.cpp
    TypeObjects.cpp
    ConstantPool.cpp
    NativeRegistry.cpp
)

target_link_libraries(ObjectSystemLib PUBLIC BytecodeSystemLib DoctorLib)

target_include_directories(ObjectSystemLib PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR})
