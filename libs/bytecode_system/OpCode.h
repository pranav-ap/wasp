#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

using ByteVector = std::vector<std::byte>;

#define OPCODE_LIST(X)                                                                             \
    /* --- System & Lifecycle --- */                                                               \
    X(NO_OP, 0)           /* No operation */                                                       \
    X(HALT, 0)            /* Stop execution */                                                     \
    X(ENTER_MODULE, 0)    /* Initialize module state */                                            \
    X(EXIT_MODULE, 1)     /* Cleanup module state */                                               \
    X(ENTER_WORKSPACE, 0) /* Initialize workspace state */                                         \
    X(EXIT_WORKSPACE, 0)  /* Cleanup workspace state */                                            \
                                                                                                   \
    /* --- Stack Manipulation --- */                                                               \
    X(POP, 0) /* [val] -> [] | Discard top of stack */                                             \
    X(DUP, 0) /* [val] -> [val, val] | Duplicate top of stack */                                   \
                                                                                                   \
    /* --- Constants & Memory --- */                                                               \
    X(LOAD_CONST, 1) /* [] -> [val] | <Constant Pool ID> (index of literal value) */               \
    X(LOAD_TRUE, 0)  /* [] -> [true] */                                                            \
    X(LOAD_FALSE, 0) /* [] -> [false] */                                                           \
    X(LOAD_NONE, 0)  /* [] -> [none] */                                                            \
                                                                                                   \
    /* --- Variables & Scoping --- */                                                              \
    X(SET_LOCAL, 1)  /* [val] -> [] | <Symbol ID> (Assign to existing local) */                    \
    X(GET_LOCAL, 1)  /* [] -> [val] | <Symbol ID> (Read local onto stack) */                       \
    X(GET_NATIVE, 1) /* [] -> [val] | <Symbol ID> */                                               \
    X(SET_MEMBER, 1) /* [obj, val] -> [] | <Member ID>  */                                         \
    X(GET_MEMBER, 1) /* [obj] -> [val] | <Member ID>  */                                           \
    X(GET_TRAIT_METHOD, 2)                                                                         \
    X(PUSH_SCOPE, 0)         /* Enter new lexical block scope */                                   \
    X(POP_SCOPE, 0)          /* Exit current lexical block scope */                                \
    X(POP_SCOPE_KEEP_TOS, 0) /* Exit current lexical block scope, preserving Top Of Stack */       \
    X(IMPORT_MODULE, 1)      /* [] -> [Module] | <Module Index> */                                 \
                                                                                                   \
    /* --- Functions, Overloads & Closures --- */                                                  \
    X(MAKE_FUNCTION, 1)           /* [code] -> [func] | <number of upvalues */                     \
    X(STORE_FUNCTION_OVERLOAD, 1) /* [func] -> [] | <Overload Group Symbol ID> */                  \
    X(RESOLVE_FUNCTION, 1)        /* [overload obj] -> [func] | <Overload Index> */                \
    X(CALL, 1)                    /* [func, args] -> [res] | <Count> (Number of args on stack) */  \
    X(RETURN, 0)                  /* Exit function with value on top of stack */                   \
    X(GET_UPVALUE, 1)             /* [] -> [val] | <Upvalue Index> */                              \
    X(SET_UPVALUE, 1)             /* [val] -> [] | <Upvalue Index> */                              \
                                                                                                   \
    /* --- Classes --- */                                                                          \
    X(BUILD_OVERLOAD_GROUP, 1) /* [closures...] -> [overload group] | <number of overloads> */     \
    X(PUSH_EMPTY_OVERLOAD_GROUP, 0)                                                                \
    X(BUILD_CLASS,                                                                                 \
      2) /* [overload groups...] -> [class blueprint] | <methods count>, <fields count> */         \
    X(PUSH_EMPTY_CLASS_BLUEPRINT, 0)                                                               \
    X(INSTANTIATE, 1) /* [class blueprint, args...] -> [instance] | <number of args> */            \
                                                                                                   \
    X(BOX, 1)                                                                                      \
    X(BUILD_TEMPLATE, 1)                                                                           \
    /* --- Arithmetic --- */                                                                       \
    X(NEGATE, 0) /* [a] -> [-a] */                                                                 \
    X(ADD, 0)    /* [a, b] -> [a+b] */                                                             \
    X(SUB, 0)    /* [a, b] -> [a-b] */                                                             \
    X(MUL, 0)    /* [a, b] -> [a*b] */                                                             \
    X(DIV, 0)    /* [a, b] -> [a/b] */                                                             \
    X(MOD, 0)    /* [a, b] -> [a%b] */                                                             \
    X(POW, 0)    /* [a, b] -> [a^b] */                                                             \
                                                                                                   \
    /* --- Comparison & Logic --- */                                                               \
    X(NOT, 0)         /* [a] -> [!a] */                                                            \
    X(EQ, 0)          /* [a, b] -> [a == b] */                                                     \
    X(NE, 0)          /* [a, b] -> [a != b] */                                                     \
    X(LT, 0)          /* [a, b] -> [a < b] */                                                      \
    X(LE, 0)          /* [a, b] -> [a <= b] */                                                     \
    X(GT, 0)          /* [a, b] -> [a > b] */                                                      \
    X(GE, 0)          /* [a, b] -> [a >= b] */                                                     \
    X(LOGICAL_AND, 0) /* [a, b] -> [a && b] */                                                     \
    X(LOGICAL_OR, 0)  /* [a, b] -> [a || b] */                                                     \
    X(COALESCE, 0)    /* [a, b] -> [a ? b] */                                                      \
                                                                                                   \
    /* --- Control Flow --- */                                                                     \
    X(JUMP, 2)          /* <Offset> | Unconditional jump forward/backward */                       \
    X(JUMP_IF_FALSE, 2) /* <Offset> | Jump if TOS is false */                                      \
    X(LOOP_ITER, 2)     /* <Offset> */                                                             \
                                                                                                   \
    /* --- Data Structures --- */                                                                  \
    X(BUILD_LIST, 1)  /* [v1...vn] -> [list] | <Count> (Number of items to pop) */                 \
    X(BUILD_TUPLE, 1) /* [v1...vn] -> [tuple] | <Count> (Number of items to pop) */                \
    X(BUILD_MAP, 1)   /* [k1, v1...] -> [map] | <Count> (Number of key-value pairs) */             \
    X(BUILD_SET, 1)   /* [v1...vn] -> [set] | <Count> (Number of items to pop) */                  \
    X(BUILD_RANGE, 1) /* [start, stop, step] -> [range] | <Count> (Arguments provided) */          \
                                                                                                   \
    /* --- Collections & Iteration --- */                                                          \
    X(GET_ITER, 0) /* [iterable] -> [iterator] | Get iterator from iterable */                     \
                                                                                                   \
    /* --- Diagnostics --- */                                                                      \
    X(ASSERT, 0) /* [cond, msg] -> [] | Throw error if cond is false */

namespace Wasp
{

enum class OpCode : uint8_t
{
#define AS_ENUM(name, arity) name,
    OPCODE_LIST(AS_ENUM)
#undef AS_ENUM
};

constexpr int get_opcode_arity(OpCode opcode)
{
    switch (opcode)
    {
#define AS_ARITY_CASE(name, arity)                                                                 \
    case OpCode::name:                                                                             \
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
#define AS_STRING_CASE(name, arity)                                                                \
    case OpCode::name:                                                                             \
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

} // namespace Wasp
