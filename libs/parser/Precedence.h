#pragma once

namespace Wasp {
    enum class Precedence : int {
        DEFINITION = 1,
        ASSIGNMENT,
        TYPE_PATTERN, // :
        OR,
        AND,
        EQUALITY, // == !=
        COMPARISON, // < > <= >= in is
        TERM, // + -
        PRODUCT, // * / %
        EXPONENT, // ^
        PREFIX, // + - ! typeof ...
        POSTFIX,
        CALL, // call() new
        MEMBER_ACCESS // . ?. ::
    };
}