add_library(ASTLib)
target_link_libraries(ASTLib PUBLIC TokenLib)
target_include_directories(ASTLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
