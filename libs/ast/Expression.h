#pragma once

#include "Token.h"

#include <string>
#include <memory>
#include <variant>
#include <vector>
#include <stack>


namespace Wasp {
    struct Expression;
    using Expression_ptr = std::shared_ptr<Expression>;
    using ExpressionVector = std::vector<Expression_ptr>;
    using ExpressionStack = std::stack<Expression_ptr>;

    struct Identifier {
        std::string name;
    };

    struct Prefix {
        Token op;
        Expression_ptr operand;
    };

    struct Infix {
        Expression_ptr left;
        Token op;
        Expression_ptr right;
    };

    struct Postfix {
        Token op;
        Expression_ptr operand;
    };

    struct Expression {
        std::variant<
            std::monostate,
            int, double, std::string, bool,
            Identifier,
            Prefix, Infix, Postfix
        > data;

        template<typename T>
        [[nodiscard]] bool is() const { return std::holds_alternative<T>(data); }

        template<typename T>
        const T &as() const { return std::get<T>(data); }
    };
}
