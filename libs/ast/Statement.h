#pragma once

#include "Expression.h"
#include <memory>
#include <variant>
#include <utility>

namespace Wasp {

struct Statement;
struct ExpressionStatement;

using Statement_ptr = std::shared_ptr<Statement>;

struct ExpressionStatement
{
    Expression_ptr expression;

    explicit ExpressionStatement(Expression_ptr expr)
        : expression(std::move(expr)) {}
};

struct Statement
{
    using StatementData = std::variant<
        std::monostate,
        ExpressionStatement
    >;

    StatementData data;
};

}