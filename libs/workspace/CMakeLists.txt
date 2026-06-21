add_library(WorkspaceLib Workspace.cpp)

target_link_libraries(WorkspaceLib PUBLIC nlohmann_json::nlohmann_json ASTLib)

target_include_directories(WorkspaceLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
