#pragma once

namespace Wasp {
    enum class Precedence : int {
        // Declarative
        DEFINITION = 1,
        ASSIGNMENT,
	    TERNARY_CONDITION,
        TYPE_PATTERN, // :

        // Logical
        OR,
        AND,
        EQUALITY, // == !=
        COMPARISON, // < > <= >= in is

        // Arithmetic
        TERM, // + -
        PRODUCT, // * / %

        // Transformational
        CAST, // as 
        EXPONENT, // ^
        PREFIX, // + - ! typeof ...

        // Structural
        POSTFIX,
        CALL, // call() new
        MEMBER_ACCESS // . ?. ::
    };
}