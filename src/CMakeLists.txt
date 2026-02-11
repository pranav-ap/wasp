project(wasp)
add_executable(wasp main.cpp)

target_link_libraries(wasp PRIVATE WorkbookLib)
target_link_libraries(wasp PRIVATE LexerLib)
