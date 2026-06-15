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

struct TypeIdentifierNode
{
    std::string name;

    Symbol_ptr symbol = nullptr;
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
    TypeAnnotationVector options;
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
struct AngularTypeNode
{
    std::string name;
    TypeAnnotationVector type_arguments;

    Symbol_ptr symbol = nullptr;
};

using TypeAnnotationVariant = std::variant<
    std::monostate,

    NoneTypeNode,
    TypeIdentifierNode,

    ListTypeNode,
    TupleTypeNode,
    SetTypeNode,
    MapTypeNode,

    VariantTypeNode,
    IntersectionTypeNode,

    FunctionTypeNode,

    AngularTypeNode>;

struct TypeAnnotation : public AstNode<TypeAnnotationVariant>
{
    using AstNode::AstNode;
};

template <typename T> inline TypeAnnotation_ptr make_type_annotation(T&& data)
{
    return std::make_shared<TypeAnnotation>(std::forward<T>(data));
}

} // namespace Wasp
