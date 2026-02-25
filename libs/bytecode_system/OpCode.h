#pragma once

#include <vector>
#include <string>
#include <cstddef>

using ByteVector = std::vector<std::byte>;

#define OPCODE_LIST(X)        \
    /* 0-arity */             \
    X(NO_OP, 0)               \
    X(START, 0)               \
    X(STOP, 0)                \
    X(FUNCTION_START, 0)      \
    X(FUNCTION_STOP, 0)       \
    X(PUSH_LOCAL_SCOPE, 0)    \
    X(POP_LOCAL_SCOPE, 0)     \
    X(POP_FROM_STACK, 0)      \
    X(UNARY_NEGATIVE, 0)      \
    X(UNARY_NOT, 0)           \
    X(ADD, 0)                 \
    X(SUBTRACT, 0)            \
    X(MULTIPLY, 0)            \
    X(DIVISION, 0)            \
    X(REMINDER, 0)            \
    X(POWER, 0)               \
    X(NOT_EQUAL, 0)           \
    X(EQUAL, 0)               \
    X(LESSER_THAN, 0)         \
    X(LESSER_THAN_EQUAL, 0)   \
    X(GREATER_THAN, 0)        \
    X(GREATER_THAN_EQUAL, 0)  \
    X(AND, 0)                 \
    X(OR, 0)                  \
    X(NULLISH_COALESE, 0)     \
    X(RETURN_VOID, 0)         \
    X(RETURN_VALUE, 0)        \
    X(YIELD_VOID, 0)          \
    X(YIELD_VALUE, 0)         \
    X(PUSH_CONSTANT_TRUE, 0)  \
    X(PUSH_CONSTANT_FALSE, 0) \
    X(ASSERT, 0)              \
    X(MAKE_ITERABLE, 0)       \
    X(CONVERT_TYPE, 0)        \
    /* 1-arity */             \
    X(PUSH_CONSTANT, 1)       \
    X(CREATE_LOCAL, 1)        \
    X(STORE_LOCAL, 1)         \
    X(LOAD_LOCAL, 1)          \
    X(MAKE_LIST, 1)           \
    X(MAKE_TUPLE, 1)          \
    X(MAKE_SET, 1)            \
    X(MAKE_MAP, 1)            \
    X(JUMP, 1)                \
    X(JUMP_IF_FALSE, 1)       \
    X(POP_JUMP, 1)            \
    X(POP_JUMP_IF_FALSE, 1)   \
    X(GET_NEXT_OR_JUMP, 1)    \
    X(LABEL, 1)               \
    /* 2-arity */             \
    X(CALL_FUNCTION, 2)       \
    X(CALL_GENERATOR, 2)      \
    X(CALL_BUILTIN_FUN, 2)

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