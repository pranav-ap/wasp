add_library(
    ParserLib
    Parser.cpp
    Parselets.cpp
    Expression.cpp
    Statement.cpp
    Importing.cpp
    Definition.cpp
    TypeAnnotation.cpp
    Branching.cpp
    Looping.cpp
    TokenPipe.cpp
    register_parselets.cpp
)

target_link_libraries(ParserLib PUBLIC ASTLib TokenLib DoctorLib)

target_include_directories(ParserLib PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR})
