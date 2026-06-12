#pragma once

#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace Wasp {

// --------------------------------------------------------------
// Forward declarations
// --------------------------------------------------------------

using StringVector = std::vector<std::string>;

struct Statement;
using Statement_ptr = std::shared_ptr<Statement>;
using StatementVector = std::vector<Statement_ptr>;

struct Expression;
using Expression_ptr = std::shared_ptr<Expression>;
using ExpressionVector = std::vector<Expression_ptr>;

struct TypeAnnotation;
using TypeAnnotation_ptr = std::shared_ptr<TypeAnnotation>;
using TypeAnnotationVector = std::vector<TypeAnnotation_ptr>;

// --------------------------------------------------------------
// AST Node Template
// ---------------------------------------------------------------

template <typename VariantType> struct AstNode {
    VariantType data;

    AstNode() = default;

    template <typename T>
        requires std::is_constructible_v<VariantType, T&&>
    AstNode(T&& val) : data(std::forward<T>(val)) {}

    template <typename T> bool is() const
    {
        return std::holds_alternative<T>(data);
    }

    template <typename T> const T& as() const
    {
        return std::get<T>(data);
    }

    template <typename T> T& as()
    {
        return std::get<T>(data);
    }
};

} // namespace Wasp
