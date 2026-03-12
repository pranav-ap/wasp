add_library(CaptainLib)
target_link_libraries(CaptainLib PUBLIC SemanticAnalyserLib CompilerLib ObjectSystemLib BytecodeSystemLib ASTLib TokenLib DoctorLib)
target_include_directories(CaptainLib PUBLIC .)
