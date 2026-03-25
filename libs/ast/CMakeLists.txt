add_library(ASTLib INTERFACE)
target_link_libraries(ASTLib INTERFACE TokenLib DoctorLib)
target_include_directories(ASTLib INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
