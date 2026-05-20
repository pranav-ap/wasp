add_library(
    ParserLib
    Parser.cpp
    ExpressionParselets.cpp
    ParserExpression.cpp
    ParserStatement.cpp
    ParserDefinition.cpp
    ParserTypeAnnotation.cpp
    ParserBranching.cpp
    ParserLooping.cpp
    TokenPipe.cpp
    register_parselets.cpp
    StupidLiar.cpp
)

target_link_libraries(ParserLib PUBLIC ASTLib TokenLib DoctorLib)

target_include_directories(ParserLib PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR})
