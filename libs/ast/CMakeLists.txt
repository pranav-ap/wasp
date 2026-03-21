add_library(ASTLib Expression.cpp)
target_link_libraries(ASTLib PUBLIC TokenLib DoctorLib)
target_include_directories(ASTLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
