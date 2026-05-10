add_library(DoctorLib Doctor.cpp)

target_link_libraries(DoctorLib PUBLIC cpptrace::cpptrace fmt::fmt)

target_include_directories(DoctorLib PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR})
