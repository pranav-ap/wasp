#pragma once

#include "AST.h"
#include "Token.h"

#include <cctype>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace Wasp
{

// =============== Block ===============

struct Block
{
    StatementVector statements;

    Statement_ptr get(int index);
    void add(Statement_ptr statement);
    int size() const;
};

// =============== Basics ===============

struct ExpressionStatement
{
    Expression_ptr expression;
};

struct Field
{
    std::string name;
    TypeNode_ptr type;
    bool is_variadic;

    Symbol_ptr symbol = nullptr;
};

using FieldVector = std::vector<Field>;

// =============== Control Flow ===============

struct Branch
{
    Block block;

    Expression_ptr test = nullptr;
    Statement_ptr alternative = nullptr;
};

struct SimpleLoop
{
    Expression_ptr test;
    TokenType style;

    Block block;
};

struct ForInLoop
{
    bool lhs_is_mutable;
    Expression_ptr lhs;
    Expression_ptr iterable;

    Block block;
};

struct Pass
{
};

struct Required
{
};

struct Native
{
};

struct Return
{
    std::optional<Expression_ptr> expression;
};

struct LoopControl
{
    TokenType type;
};

// =============== Definitions ===============

struct TypeAliasDefinition
{
    std::string name;

    FieldVector generics;
    TypeNode_ptr ref_type;

    Symbol_ptr symbol = nullptr;
};

struct EnumDefinition
{
    std::string name;
    FieldVector generics;

    StringVector members;
    std::vector<EnumDefinition> nested_enums;

    Symbol_ptr symbol = nullptr;
};

struct FunctionDefinition
{
    std::string name;
    FieldVector generics;

    FieldVector parameters;
    TypeNode_ptr return_type;

    Block block;

    bool is_pure;

    Symbol_ptr symbol = nullptr;
};

using FunctionDefinitionVector = std::vector<FunctionDefinition>;

struct MethodDefinition
{
    std::string name;

    FieldVector parameters;
    TypeNode_ptr return_type;

    Block block;

    bool is_pure;
    bool is_shared;

    Symbol_ptr symbol = nullptr;

    Symbol_ptr owner_symbol = nullptr;

    Symbol_ptr self_context_symbol = nullptr;
    Symbol_ptr our_context_symbol = nullptr;
};

using MethodDefinitionVector = std::vector<MethodDefinition>;

struct OperatorDefinition
{
    std::string name;
    FieldVector generics;

    TokenType op_type;
    TokenType fixity;

    FieldVector operands;
    TypeNode_ptr return_type;

    Block block;

    Symbol_ptr symbol = nullptr;
};

struct RecordDefinition
{
    std::string name;

    FieldVector fields;

    Symbol_ptr symbol = nullptr;
};

struct TypeDefinition
{
    std::string name;

    FieldVector generics;
    FieldVector fields;
    MethodDefinitionVector methods;
    TypeNodeVector traits;

    Symbol_ptr symbol = nullptr;

    explicit TypeDefinition() = default;

    explicit TypeDefinition(
        std::string name,
        FieldVector generics,
        FieldVector fields,
        MethodDefinitionVector methods,
        TypeNodeVector traits
    )
        : name(std::move(name)), generics(std::move(generics)),
          fields(std::move(fields)), methods(std::move(methods)),
          traits(std::move(traits))
    {
    }
};

struct ClassDefinition : public TypeDefinition
{
    using TypeDefinition::TypeDefinition;
};

struct TraitDefinition : public TypeDefinition
{
    using TypeDefinition::TypeDefinition;
};

struct PrimitiveDefinition : public TypeDefinition
{
    using TypeDefinition::TypeDefinition;
};

// =============== Imports ===============

struct ImportAsPair
{
    std::string name;
    std::optional<std::string> alias = std::nullopt;
};

struct Import
{
    std::optional<TokenType> access_modifier;
    int jumps;

    StringVector path;

    std::optional<std::string> module_alias;

    bool expose_all;
    std::vector<ImportAsPair> exposed_names;

    Symbol_ptr module_symbol = nullptr;

    // filled by DependencyCrawler
    std::filesystem::path module_path = std::filesystem::path();
};

// --- Variant  ---

using StatementVariant = std::variant<
    std::monostate,

    Import,
    ExpressionStatement,

    TypeAliasDefinition,
    EnumDefinition,

    FunctionDefinition,
    MethodDefinition,

    OperatorDefinition,

    RecordDefinition,
    ClassDefinition,
    TraitDefinition,
    PrimitiveDefinition,

    Block,

    Branch,

    SimpleLoop,
    ForInLoop,
    LoopControl,

    Return,

    Pass,
    Required,
    Native>;

struct Statement : public AstNode<StatementVariant>
{
    using AstNode::AstNode;
};

template <typename T> inline Statement_ptr make_statement(T&& data)
{
    return std::make_shared<Statement>(std::forward<T>(data));
}

} // namespace Wasp
