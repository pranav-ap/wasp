; clang ./play/a.ll -o ./play/a
; ./play/a
; echo "Exit code: $?"

target triple = "x86_64-pc-linux-gnu"

; Globals are prefixed with @
@variable = global i32 21

; constant occupies memory
@hello = internal constant [6 x i8] c"hello\00"

; STRUCT

; struct Foo { size_t x; double y; }
%Foo = type { i64, double }

; struct FooBar { Foo x; char* c; Foo* y; }
%FooBar = type { %Foo, i8*, %Foo* }

; compiler knows it exists, but doesn't know its size or contents
%Bar = type opaque

%Dog = type { i32, i8*, double }


define i32 @max(i32 %a, i32 %b) {
    entry:
        %0 = icmp sgt i32 %a, %b
        br i1 %0, label %btrue, label %bfalse

    btrue:
        br label %end

    bfalse:
        br label %end

    end:
        ; selects a value depending on which previous block jumped here
        %retval = phi i32 [%a, %btrue], [%b, %bfalse]
        ret i32 %retval
}

define i32 @main() {
    ; GLOBALS

    ; load the global variable
    %1 = load i32, ptr @variable

    %2 = mul i32 %1, 2

    ; LOCALS

    ; store instruction to write to global variable
    store i32 %2, ptr @variable

    ; compute and stre result in virtual register %reg
    %reg = add i32 4, 2

    ; allocates space on stack for an i32
    ; stores the pointer to it in %stack
    %stack = alloca i32

    ; CONSTANTS

    ; inline
    %3 = add i32 %1, 17

    ; GEP

    %dog = alloca %Dog

    ; To access the b member
    %4 = getelementptr %Dog, %Dog* %dog, i32 0, i32 1

    ; CALL

    %x = add i32 1, 2
    %y = mul i32 2, 2
    %max_result = call i32 @max(i32 %x, i32 %y)

    ret i32 %max_result
}
