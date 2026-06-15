add_library(
    SemanticsLib
    Type.cpp
    Type_utils.cpp
    Symbol.cpp
    SymbolFactory.cpp
    SymbolScope.cpp
    Hoisting.cpp
    NameResolution.cpp
    TypeCollection.cpp
    TypeSystem.cpp
    SemanticsAnalyzer.cpp
)

target_link_libraries(SemanticsLib PUBLIC ASTLib TokenLib WorkspaceLib DoctorLib)

target_include_directories(SemanticsLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
