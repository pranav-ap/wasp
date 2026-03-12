add_library(CompilerLib Compiler.cpp InstructionPrinter.cpp)
target_link_libraries(CompilerLib PUBLIC ObjectSystemLib BytecodeSystemLib SymbolLib ASTLib TokenLib DoctorLib)
target_include_directories(CompilerLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
