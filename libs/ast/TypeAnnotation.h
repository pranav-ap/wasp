#pragma once

#include "AST.h"
#include "Token.h"

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <variant>

namespace Wasp {

struct AnyTypeNode {};
struct NoneTypeNode {};
struct IntTypeNode {};
struct FloatTypeNode {};
struct StringTypeNode {};
struct BoolTypeNode {};

struct IntLiteralTypeNode {
    int value;
};
struct FloatLiteralTypeNode {
    double value;
};
struct StringLiteralTypeNode {
    std::string value;
};
struct BoolLiteralTypeNode {
    bool value;
};

struct TypeIdentifierNode {
    std::string name;
};

struct ListTypeNode;
struct TupleTypeNode;
struct SetTypeNode;
struct MapTypeNode;
struct VariantTypeNode;
struct FunctionTypeNode;
struct RecordTypeNode;

struct ListTypeNode {
    TypeAnnotation_ptr element_type;
    explicit ListTypeNode() = default;
    explicit ListTypeNode(TypeAnnotation_ptr type) : element_type(std::move(type)) {}
};

struct TupleTypeNode {
    TypeAnnotationVector element_types;
    explicit TupleTypeNode() = default;
    explicit TupleTypeNode(TypeAnnotationVector types) : element_types(std::move(types)) {}
};

struct SetTypeNode {
    TypeAnnotation_ptr element_type;
    explicit SetTypeNode() = default;
    explicit SetTypeNode(TypeAnnotation_ptr type) : element_type(std::move(type)) {}
};

struct MapTypeNode {
    TypeAnnotation_ptr key_type;
    TypeAnnotation_ptr value_type;

    explicit MapTypeNode() = default;
    explicit MapTypeNode(TypeAnnotation_ptr key, TypeAnnotation_ptr value)
        : key_type(std::move(key)), value_type(std::move(value)) {}
};

struct VariantTypeNode {
    TypeAnnotationVector types;
    explicit VariantTypeNode() = default;
    explicit VariantTypeNode(TypeAnnotationVector types) : types(std::move(types)) {}
};

struct FunctionTypeNode {
    TypeAnnotationVector input_types;
    TypeAnnotation_ptr return_type;

    explicit FunctionTypeNode() = default;
    explicit FunctionTypeNode(TypeAnnotationVector inputs, TypeAnnotation_ptr ret)
        : input_types(std::move(inputs)), return_type(std::move(ret)) {}
};

struct RecordTypeNode {
    std::map<std::string, TypeAnnotation_ptr> members;

    explicit RecordTypeNode() = default;
    explicit RecordTypeNode(std::map<std::string, TypeAnnotation_ptr> members)
        : members(std::move(members)) {}
};

// Type Anno

// 1. Define the variant payload FIRST
using TypeAnnotationVariant = std::variant<
    std::monostate,

    AnyTypeNode,
    NoneTypeNode,

    IntTypeNode,
    FloatTypeNode,
    StringTypeNode,
    BoolTypeNode,

    IntLiteralTypeNode,
    FloatLiteralTypeNode,
    StringLiteralTypeNode,
    BoolLiteralTypeNode,

    TypeIdentifierNode,

    std::shared_ptr<ListTypeNode>,
    std::shared_ptr<TupleTypeNode>,
    std::shared_ptr<SetTypeNode>,
    std::shared_ptr<MapTypeNode>,
    std::shared_ptr<VariantTypeNode>,
    std::shared_ptr<FunctionTypeNode>,
    std::shared_ptr<RecordTypeNode>>;

struct TypeAnnotation : public AstNode<TypeAnnotationVariant> {
    using AstNode::AstNode;

    Token start_token;
    Token end_token;
};

template <typename T> inline TypeAnnotation_ptr make_type_annotation(T&& data) {
    return std::make_shared<TypeAnnotation>(std::forward<T>(data));
}

template <typename T>
inline TypeAnnotation_ptr make_type_annotation(T&& data, Token start_token, Token end_token) {
    auto type_node = std::make_shared<TypeAnnotation>(std::forward<T>(data));
    type_node->start_token = std::move(start_token);
    type_node->end_token = std::move(end_token);
    return type_node;
}

} // namespace Wasp
