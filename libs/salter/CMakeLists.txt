add_library(SalterLib Salter.cpp)

target_include_directories(SalterLib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${LLVM_INCLUDE_DIRS}
)

target_link_directories(SalterLib PUBLIC ${LLVM_LIBRARY_DIRS})

target_link_libraries(SalterLib PUBLIC
    ${LLVM_LIBS}
    SemanticsLib
    ASTLib
    TokenLib
    WorkspaceLib
    DoctorLib
)
