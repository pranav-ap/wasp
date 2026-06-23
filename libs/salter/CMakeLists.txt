add_library(SalterLib Salter.cpp)

target_include_directories(SalterLib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(SalterLib PUBLIC
    SemanticsLib
    ASTLib
    TokenLib
    WorkspaceLib
    DoctorLib
)
