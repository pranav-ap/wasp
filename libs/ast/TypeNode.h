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
    Expression_ptr literal;
};

struct TypeIdentifierNode
{
    std::string name;

    Symbol_ptr symbol = nullptr;
};

struct ListTypeNode
{
    TypeNode_ptr element_type;
};

struct TupleTypeNode
{
    TypeNodeVector element_types;
};

struct SetTypeNode
{
    TypeNode_ptr element_type;
};

struct MapTypeNode
{
    TypeNode_ptr key_type;
    TypeNode_ptr value_type;
};

struct VariantTypeNode
{
    TypeNodeVector options;
};

struct IntersectionTypeNode
{
    TypeNodeVector types;
};

struct FunctionTypeNode
{
    TypeNodeVector parameter_types;
    TypeNode_ptr return_type;
};

// Foo<T>
struct AngularTypeNode
{
    std::string name;
    TypeNodeVector type_arguments;
};

using TypeNodeVariant = std::variant<
    std::monostate,

    NoneTypeNode,
    LiteralTypeNode,
    TypeIdentifierNode,
    AngularTypeNode,

    ListTypeNode,
    TupleTypeNode,
    SetTypeNode,
    MapTypeNode,

    VariantTypeNode,
    IntersectionTypeNode,

    FunctionTypeNode>;

struct TypeNode : public AstNode<TypeNodeVariant>
{
    using AstNode::AstNode;
};

template <typename T> inline TypeNode_ptr make_type_node(T&& data)
{
    return std::make_shared<TypeNode>(std::forward<T>(data));
}

} // namespace Wasp
