add_library(ObjectSystemLib Objects.cpp ConstantPool.cpp ObjectsUtils.cpp NativeRegistry.cpp)
target_link_libraries(ObjectSystemLib PUBLIC BytecodeSystemLib)
target_include_directories(ObjectSystemLib PUBLIC .)
