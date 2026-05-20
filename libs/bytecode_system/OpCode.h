#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

using ByteVector = std::vector<std::byte>;

#define OPCODE_LIST(X)                                                                             \
    /* --- System & Lifecycle --- */                                                               \
    X(NO_OP, 0)                                                                                    \
    X(HALT, 0)                                                                                     \
    X(ENTER_MODULE, 0)                                                                             \
    X(EXIT_MODULE, 1)                                                                              \
    X(ENTER_WORKSPACE, 0)                                                                          \
    X(EXIT_WORKSPACE, 0)                                                                           \
    /* --- Arithmetic --- */                                                                       \
    X(NEGATE, 0)                                                                                   \
    X(ADD, 0)                                                                                      \
    X(SUB, 0)                                                                                      \
    X(MUL, 0)                                                                                      \
    X(DIV, 0)                                                                                      \
    X(MOD, 0)                                                                                      \
    X(POW, 0)                                                                                      \
    /* --- Comparison & Logic --- */                                                               \
    X(NOT, 0)                                                                                      \
    X(EQ, 0)                                                                                       \
    X(NE, 0)                                                                                       \
    X(LT, 0)                                                                                       \
    X(LE, 0)                                                                                       \
    X(GT, 0)                                                                                       \
    X(GE, 0)                                                                                       \
    X(LOGICAL_AND, 0)                                                                              \
    X(LOGICAL_OR, 0)                                                                               \
    X(COALESCE, 0)                                                                                 \
    /* --- Control Flow --- */                                                                     \
    X(JUMP, 2)          /* <address> */                                                            \
    X(JUMP_IF_FALSE, 2) /* <address> */                                                            \
    X(LOOP_ITER, 2)     /* <address> */                                                            \
    /* --- Data Structures --- */                                                                  \
    X(BUILD_LIST, 1)  /*  Number of items to pop */                                                \
    X(BUILD_TUPLE, 1) /* Number of items to pop */                                                 \
    X(BUILD_MAP, 1)   /* Number of key-value pairs */                                              \
    X(BUILD_SET, 1)   /* Number of items to pop */                                                 \
    X(BUILD_RANGE, 1)                                                                              \
    /* --- Collections & Iteration --- */                                                          \
    X(GET_ITER, 0) /* [iterable] -> [iterator] | Get iterator from iterable */                     \
    /* --- Diagnostics --- */                                                                      \
    X(ASSERT, 0) /* [cond, msg] -> [] | Throw error if cond is false */                            \
    /* --- Stack Manipulation --- */                                                               \
    X(POP, 0) /* [val] -> []  */                                                                   \
    X(DUP, 0) /* [val] -> [val, val]  */                                                           \
    /* --- Constants & Memory --- */                                                               \
    X(LOAD_CONSTANT, 1) /* [] -> [Object from pool] | <Constant Pool ID> */                        \
    X(LOAD_TRUE, 0)     /* [] -> [true] */                                                         \
    X(LOAD_FALSE, 0)    /* [] -> [false] */                                                        \
    X(LOAD_NONE, 0)     /* [] -> [none] */                                                         \
    /* --- Variables & Access --- */                                                               \
    X(SET_LOCAL, 1)                                                                                \
    X(GET_LOCAL, 1)                                                                                \
    X(GET_NATIVE, 1)                                                                               \
    X(GET_FIELD, 1)        /* <member index> */                                                    \
    X(SET_FIELD, 1)        /* <member index> */                                                    \
    X(GET_MEMBER, 1)       /* <member index> */                                                    \
    X(SET_MEMBER, 1)       /* <member index> */                                                    \
    X(GET_FUNCTION, 1)     /* <overload index> */                                                  \
    X(GET_CLASS_METHOD, 2) /* <member index> <overload index>*/                                    \
    X(GET_TRAIT_METHOD, 2) /* <member index> <overload index>*/                                    \
    /* --- Scopes --- */                                                                           \
    X(PUSH_SCOPE, 0)                                                                               \
    X(POP_SCOPE, 0)                                                                                \
    X(POP_SCOPE_KEEP_TOS, 0)                                                                       \
    /* --- Module --- */                                                                           \
    X(IMPORT_MODULE, 1)         /* <module index> */                                               \
    X(UNPACK_MODULE_MEMBERS, 2) /* <module slot> <count> | followed by <count> member indices */   \
    /* --- Functions --- */                                                                        \
    X(BUILD_FUNCTION, 1)          /* [CodeObject] -> [Func BP] | <upvalues count> */               \
    X(STORE_FUNCTION_OVERLOAD, 1) /* [Func BP] -> [] | <Overload Group Symbol ID> */               \
    X(CALL, 1)                    /* [func, args] -> [res] | <Count> (Number of args on stack) */  \
    X(RETURN, 0)                  /* Exit function with value on top of stack */                   \
    X(GET_UPVALUE, 1)             /* [] -> [val] | <Upvalue Index> */                              \
    X(SET_UPVALUE, 1)             /* [val] -> [] | <Upvalue Index> */                              \
    /* --- Classes --- */                                                                          \
    X(BUILD_OVERLOAD_GROUP, 1) /* [closures...] -> [overload group] | <number of overloads> */     \
    X(PUSH_EMPTY_OVERLOAD_GROUP, 0)                                                                \
    X(BUILD_CLASS, 1) /* [overload groups...] -> [Class BP] | <methods count> */                   \
    X(PUSH_EMPTY_CLASS_BLUEPRINT, 0)                                                               \
    X(INSTANTIATE, 1) /* [args..., Class BP] -> [instance] | <args count> */                       \
    X(BOX, 1)                                                                                      \
    /* --- Template --- */                                                                         \
    X(BUILD_TEMPLATE, 1)

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
