#pragma once

#include "AST.h"
#include "Token.h"

#include <memory>
#include <string>
#include <utility>
#include <variant>

namespace Wasp
{

struct PossiblyNative
{
    bool is_native = false;
};

struct NoneTypeNode
{
};

struct IntLiteralTypeNode
{
    int value;
};

struct FloatLiteralTypeNode
{
    double value;
};

struct StringLiteralTypeNode
{
    std::string value;
};

struct BoolLiteralTypeNode
{
    bool value;
};

struct TypeIdentifierNode : public PossiblyNative
{
    std::string name;

    explicit TypeIdentifierNode() = default;

    explicit TypeIdentifierNode(std::string name, bool is_native = false)
        : name(std::move(name)), PossiblyNative{is_native}
    {
    }
};

struct ListTypeNode : public PossiblyNative
{
    TypeAnnotation_ptr element_type;
    explicit ListTypeNode() = default;

    explicit ListTypeNode(TypeAnnotation_ptr type, bool is_native = false)
        : element_type(std::move(type)), PossiblyNative{is_native}
    {
    }
};

struct TupleTypeNode : public PossiblyNative
{
    TypeAnnotationVector element_types;
    explicit TupleTypeNode() = default;

    explicit TupleTypeNode(TypeAnnotationVector types, bool is_native = false)
        : element_types(std::move(types)), PossiblyNative{is_native}
    {
    }
};

struct SetTypeNode : public PossiblyNative
{
    TypeAnnotation_ptr element_type;
    explicit SetTypeNode() = default;

    explicit SetTypeNode(TypeAnnotation_ptr type, bool is_native = false)
        : element_type(std::move(type)), PossiblyNative{is_native}
    {
    }
};

struct MapTypeNode : public PossiblyNative
{
    TypeAnnotation_ptr key_type;
    TypeAnnotation_ptr value_type;

    explicit MapTypeNode() = default;

    explicit MapTypeNode(
        TypeAnnotation_ptr key,
        TypeAnnotation_ptr value,
        bool is_native = false
    )
        : key_type(std::move(key)), value_type(std::move(value)),
          PossiblyNative{is_native}
    {
    }
};

struct VariantTypeNode : public PossiblyNative
{
    TypeAnnotationVector types;
    explicit VariantTypeNode() = default;

    explicit VariantTypeNode(TypeAnnotationVector types) : types(std::move(types))
    {
    }
};

struct FunctionTypeNode
{
    TypeAnnotationVector input_types;
    TypeAnnotation_ptr return_type;

    explicit FunctionTypeNode() = default;

    explicit FunctionTypeNode(TypeAnnotationVector inputs, TypeAnnotation_ptr ret)
        : input_types(std::move(inputs)), return_type(std::move(ret))
    {
    }
};

struct RecordTypeNode
{
    StatementVector fields;

    explicit RecordTypeNode() = default;

    explicit RecordTypeNode(StatementVector fields) : fields(std::move(fields))
    {
    }
};

// Foo<T>
struct TemplateAngularTypeNode
{
    TypeAnnotation_ptr base_node;
    TypeAnnotationVector angular_nodes;

    explicit TemplateAngularTypeNode() = default;

    explicit TemplateAngularTypeNode(
        TypeAnnotation_ptr base_node,
        TypeAnnotationVector angular_nodes
    )
        : base_node(std::move(base_node)), angular_nodes(std::move(angular_nodes))
    {
    }
};

using TypeAnnotationVariant = std::variant<
    std::monostate,

    NoneTypeNode,

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
    std::shared_ptr<RecordTypeNode>,
    std::shared_ptr<TemplateAngularTypeNode>>;

struct TypeAnnotation : public AstNode<TypeAnnotationVariant>
{
    using AstNode::AstNode;

    Token start_token;
    Token end_token;
};

template <typename T> inline TypeAnnotation_ptr make_type_annotation(T&& data)
{
    return std::make_shared<TypeAnnotation>(std::forward<T>(data));
}

template <typename T>
inline TypeAnnotation_ptr make_type_annotation(
    T&& data,
    Token start_token,
    Token end_token
)
{
    auto type_node = std::make_shared<TypeAnnotation>(std::forward<T>(data));
    type_node->start_token = std::move(start_token);
    type_node->end_token = std::move(end_token);
    return type_node;
}

} // namespace Wasp
