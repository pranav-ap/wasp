; clang ./play/a.ll -o ./play/a
; ./play/a
; echo "Exit code: $?"

target triple = "x86_64-pc-linux-gnu"

@variable = global i32 21

define i32 @main() {
    %1 = load i32, ptr @variable
    %2 = mul i32 %1, 2
    store i32 %2, ptr @variable
    ret i32 %2
}
