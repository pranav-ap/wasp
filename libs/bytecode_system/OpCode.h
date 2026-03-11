#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

using ByteVector = std::vector<std::byte>;

#define OPCODE_LIST(X)                                                               \
    /* --- System & Lifecycle --- */                                                 \
    X(NO_OP, 0)        /* No operation */                                            \
    X(HALT, 0)         /* Stop execution */                                          \
    X(ENTER_MODULE, 0) /* Initialize module state */                                 \
    X(EXIT_MODULE, 0)  /* Cleanup module state */                                    \
    X(ENTER_WORKSPACE, 0) /* Initialize workspace state */                           \
    X(EXIT_WORKSPACE, 0)  /* Cleanup workspace state */                              \
    /* --- Stack Manipulation --- */                                                 \
    X(POP, 0) /* [val] -> [] | Discard top of stack */                               \
    X(DUP, 0) /* [val] -> [val, val] | Duplicate top of stack */                     \
    /* --- Constants & Memory --- */                                                 \
    X(LOAD_CONST, 1) /* [] -> [val] | Load constant from pool at index */            \
    X(LOAD_TRUE, 0)  /* [] -> [true] */                                              \
    X(LOAD_FALSE, 0) /* [] -> [false] */                                             \
    X(LOAD_NONE, 0)  /* [] -> [none] */                                              \
    /* --- Variables & Scoping --- */                                                \
    X(DEFINE_LOCAL, 1) /* [val] -> [] | Create local variable in current slot */     \
    X(SET_LOCAL, 1)    /* [val] -> [] | Assign value to existing local slot */       \
    X(GET_LOCAL, 1)    /* [] -> [val] | Read value from local slot onto stack */     \
    X(GET_NATIVE, 1)   /* [] -> [val] | Read value from native registry onto stack */     \
    X(PUSH_SCOPE, 0)   /* Enter new lexical block scope */                           \
    X(POP_SCOPE, 0)                                                                  \
    /* Exit current lexical block scope */ /* --- Arithmetic --- */                  \
    X(NEGATE, 0)                           /* [a] -> [-a] */                         \
    X(ADD, 0)                              /* [a, b] -> [a+b] */                     \
    X(SUB, 0)                              /* [a, b] -> [a-b] */                     \
    X(MUL, 0)                              /* [a, b] -> [a*b] */                     \
    X(DIV, 0)                              /* [a, b] -> [a/b] */                     \
    X(MOD, 0)                              /* [a, b] -> [a%b] */                     \
    X(POW, 0)                                                                        \
    /* [a, b] -> [a^b] */ /* --- Comparison & Logic --- */                           \
    X(NOT, 0)             /* [a] -> [!a] */                                          \
    X(EQ, 0)              /* [a, b] -> [a == b] */                                   \
    X(NE, 0)              /* [a, b] -> [a != b] */                                   \
    X(LT, 0)              /* [a, b] -> [a < b] */                                    \
    X(LE, 0)              /* [a, b] -> [a <= b] */                                   \
    X(GT, 0)              /* [a, b] -> [a > b] */                                    \
    X(GE, 0)              /* [a, b] -> [a >= b] */                                   \
    X(LOGICAL_AND, 0)     /* [a, b] -> [a && b] */                                   \
    X(LOGICAL_OR, 0)      /* [a, b] -> [a || b] */                                   \
    X(COALESCE, 0)                                                                   \
    /* [a, b] -> [a ? b] */ /* --- Control Flow --- */                               \
    X(JUMP, 2)              /* Unconditional jump to offset */                       \
    X(JUMP_IF_FALSE, 2)     /* Jump to offset if TOS is false */                     \
    X(LOOP_ITER, 2)         /* GET_NEXT_OR_JUMP | Advance iterator or jump to end */ \
    /* --- Data Structures --- */                                                    \
    X(BUILD_LIST, 1)  /* [v1...vn] -> [list] | Build list from N stack elements */   \
    X(BUILD_TUPLE, 1) /* [v1...vn] -> [tuple] */                                     \
    X(BUILD_MAP, 1)   /* [k1, v1...kn, vn] -> [map] */                               \
    X(BUILD_SET, 1)                                                                  \
    X(BUILD_RANGE, 1)                                                                \
    /* --- Collections & Iteration --- */                                            \
    X(GET_ITER, 0) /* [iterable] -> [iterator] | Get iterator from iterable */       \
    /* --- Functions & Closures --- */                                               \
    X(MAKE_FUNCTION, 1) /* [code_obj] -> [func_obj] | Wrap compiled code */          \
    X(CALL, 1)          /* [func, args...] -> [result] | Invoke top of stack */      \
    X(RETURN, 0)        /* Exit function with value on top of stack */               \
    X(YIELD, 0)         /* Suspend generator with value on top of stack */           \
    X(GET_UPVALUE, 1)   /* [ ] -> [val] | Get captured variable from closure */      \
    X(SET_UPVALUE, 1)   /* [val] -> [ ] | Set captured variable in closure */        \
    X(CLOSE_UPVALUE, 0) /* Move a local variable from stack to heap */               \
    /* --- Diagnostics --- */                                                        \
    X(ASSERT, 0) /* [cond, msg] -> [] | Throw error if cond is false */

namespace Wasp
{

enum class OpCode : uint8_t {
#define AS_ENUM(name, arity) name,
  OPCODE_LIST(AS_ENUM)
#undef AS_ENUM
};

    constexpr int get_opcode_arity(OpCode opcode)
    {
        switch (opcode)
        {
#define AS_ARITY_CASE(name, arity) \
    case OpCode::name:             \
        return arity;
            OPCODE_LIST(AS_ARITY_CASE)
#undef AS_ARITY_CASE
        default:
            return -1;
        }
    }

    constexpr int get_opcode_arity(std::byte opcode)
    {
        return get_opcode_arity(static_cast<OpCode>(opcode));
    }

    inline std::string stringify_opcode(OpCode opcode)
    {
        switch (opcode)
        {
#define AS_STRING_CASE(name, arity) \
    case OpCode::name:              \
        return #name;
            OPCODE_LIST(AS_STRING_CASE)
#undef AS_STRING_CASE
        default:
            return "";
        }
    }

    inline std::string stringify_opcode(std::byte opcode)
    {
        return stringify_opcode(static_cast<OpCode>(opcode));
    }

}