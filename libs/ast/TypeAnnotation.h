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

struct LiteralTypeNode
{
    Expression_ptr value;
};

struct TypeIdentifierNode
{
    std::string name;
};

struct ListTypeNode
{
    TypeAnnotation_ptr element_type;
};

struct TupleTypeNode
{
    TypeAnnotationVector element_types;
};

struct SetTypeNode
{
    TypeAnnotation_ptr element_type;
};

struct MapTypeNode
{
    TypeAnnotation_ptr key_type;
    TypeAnnotation_ptr value_type;
};

struct VariantTypeNode
{
    TypeAnnotationVector types;
};

struct IntersectionTypeNode
{
    TypeAnnotationVector types;
};

struct FunctionTypeNode
{
    TypeAnnotationVector input_types;
    TypeAnnotation_ptr return_type;
};

// Foo<T>
struct TemplateAngularTypeNode
{
    TypeAnnotation_ptr base_type;
    TypeAnnotationVector type_arguments;
};

using TypeAnnotationVariant = std::variant<
    std::monostate,

    NoneTypeNode,
    LiteralTypeNode,
    TypeIdentifierNode,

    ListTypeNode,
    TupleTypeNode,
    SetTypeNode,
    MapTypeNode,
    VariantTypeNode,
    IntersectionTypeNode,
    FunctionTypeNode,
    TemplateAngularTypeNode>;

struct TypeAnnotation : public AstNode<TypeAnnotationVariant>
{
    using AstNode::AstNode;
};

template <typename T> inline TypeAnnotation_ptr make_type_annotation(T&& data)
{
    return std::make_shared<TypeAnnotation>(std::forward<T>(data));
}

} // namespace Wasp
