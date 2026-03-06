add_library(ObjectSystemLib Objects.cpp ConstantPool.cpp ObjectsUtils.cpp)
target_link_libraries(ObjectSystemLib PUBLIC BytecodeSystemLib)
target_include_directories(ObjectSystemLib PUBLIC .)
