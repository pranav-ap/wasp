#pragma once

namespace Wasp
{
    enum class Precedence : int
    {
        COMMA = 1,
        DEFINITION,
        ASSIGNMENT,
        PIPE, // ~
        TERNARY_CONDITION,
        TYPE_PATTERN, // :

        // Logical
        OR,
        AND,
        EQUALITY,   // == !=
        COMPARISON, // < > <= >= in is

        RANGE,

        // Arithmetic
        TERM,    // + -
        PRODUCT, // * / %

        // Transformational
        CAST,     // as
        EXPONENT, // ^
        PREFIX,   // + - typeof

        // Structural
        POSTFIX,
        CALL,         // call() new delete
        MEMBER_ACCESS // . ?.
    };
}