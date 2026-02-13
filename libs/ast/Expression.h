#pragma once

#include "Token.h"

#include <string>
#include <memory>
#include <variant>
#include <vector>
#include <map>
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

    struct SequenceLiteral {
        ExpressionVector expressions;
        SequenceLiteral(ExpressionVector expressions)
            : expressions(expressions) {};
    };

    struct ListLiteral : public SequenceLiteral {
        ListLiteral(ExpressionVector expressions)
            : SequenceLiteral(expressions) {};
    };

    struct TupleLiteral : public SequenceLiteral {
        TupleLiteral(ExpressionVector expressions)
            : SequenceLiteral(expressions) {};
    };

    struct SetLiteral : public SequenceLiteral {
        SetLiteral(ExpressionVector expressions)
            : SequenceLiteral(expressions) {};
    };

    struct MapLiteral {
        std::map<Expression_ptr, Expression_ptr> pairs;

        MapLiteral(std::map<Expression_ptr, Expression_ptr> pairs)
            : pairs(pairs) {};
    };


    struct Expression {
        std::variant<
            std::monostate,
            int, double, std::string, bool,
            Identifier,
            Prefix, Infix, Postfix,
            ListLiteral, TupleLiteral, SetLiteral, MapLiteral
        > data;

        template<typename T>
        [[nodiscard]] bool is() const { return std::holds_alternative<T>(data); }

        template<typename T>
        const T &as() const { return std::get<T>(data); }
    };
}
