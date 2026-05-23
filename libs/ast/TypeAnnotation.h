#pragma once

#include "AST.h"

#include <memory>
#include <string>
#include <utility>
#include <variant>

namespace Wasp
{

struct NoneTypeNode
{
};

struct NativeTypeNode
{
    TypeAnnotation_ptr underlying_type;

    explicit NativeTypeNode() = default;

    explicit NativeTypeNode(TypeAnnotation_ptr underlying_type)
        : underlying_type(std::move(underlying_type))
    {
    }
};

struct LiteralTypeNode
{
    Expression_ptr literal;
};

struct TypeIdentifierNode
{
    std::string name;

    explicit TypeIdentifierNode() = default;

    explicit TypeIdentifierNode(std::string name) : name(std::move(name))
    {
    }
};

struct ListTypeNode
{
    TypeAnnotation_ptr element_type;

    explicit ListTypeNode() = default;

    explicit ListTypeNode(TypeAnnotation_ptr type)
        : element_type(std::move(type))
    {
    }
};

struct TupleTypeNode
{
    TypeAnnotationVector element_types;

    explicit TupleTypeNode() = default;

    explicit TupleTypeNode(TypeAnnotationVector types)
        : element_types(std::move(types))
    {
    }
};

struct SetTypeNode
{
    TypeAnnotation_ptr element_type;

    explicit SetTypeNode() = default;

    explicit SetTypeNode(TypeAnnotation_ptr type)
        : element_type(std::move(type))
    {
    }
};

struct MapTypeNode
{
    TypeAnnotation_ptr key_type;
    TypeAnnotation_ptr value_type;

    explicit MapTypeNode() = default;

    explicit MapTypeNode(TypeAnnotation_ptr key, TypeAnnotation_ptr value)
        : key_type(std::move(key)), value_type(std::move(value))
    {
    }
};

struct VariantTypeNode
{
    TypeAnnotationVector types;

    explicit VariantTypeNode() = default;

    explicit VariantTypeNode(TypeAnnotationVector types) : types(std::move(types))
    {
    }
};

struct IntersectionTypeNode
{
    TypeAnnotationVector types;

    explicit IntersectionTypeNode() = default;

    explicit IntersectionTypeNode(TypeAnnotationVector types) : types(std::move(types))
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

    NativeTypeNode,

    NoneTypeNode,
    LiteralTypeNode,
    TypeIdentifierNode,

    std::shared_ptr<ListTypeNode>,
    std::shared_ptr<TupleTypeNode>,
    std::shared_ptr<SetTypeNode>,
    std::shared_ptr<MapTypeNode>,
    std::shared_ptr<VariantTypeNode>,
    std::shared_ptr<IntersectionTypeNode>,
    std::shared_ptr<FunctionTypeNode>,
    std::shared_ptr<RecordTypeNode>,
    std::shared_ptr<TemplateAngularTypeNode>>;

struct TypeAnnotation : public AstNode<TypeAnnotationVariant>
{
    using AstNode::AstNode;
};

template <typename T> inline TypeAnnotation_ptr make_type_annotation(T&& data)
{
    return std::make_shared<TypeAnnotation>(std::forward<T>(data));
}

} // namespace Wasp
