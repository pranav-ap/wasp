add_library(
    ASTLib
    Statement.cpp
    ASTCloner.cpp
    ASTPrinter.cpp
    Solidifier.cpp
)

target_link_libraries(ASTLib PUBLIC nlohmann_json::nlohmann_json TokenLib DoctorLib)

target_include_directories(ASTLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
