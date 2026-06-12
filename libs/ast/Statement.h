#pragma once

#include "AST.h"
#include "Token.h"

#include <cctype>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace Wasp
{

// =============== Basics ===============

struct ExpressionStatement
{
    Expression_ptr expression;
};

struct Field
{
    std::string name;
    TypeAnnotation_ptr type;
    bool is_variadic;
};

using FieldVector = std::vector<Field>;

struct Block
{
    StatementVector statements;
};

// =============== Control Flow ===============

struct Branch
{
    Block block;

    Expression_ptr test = nullptr;
    Statement_ptr alternative = nullptr;
};

struct SimpleLoop
{
    Expression_ptr condition;
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

    TypeAnnotation_ptr ref_type;
};

struct EnumDefinition
{
    std::string name;
    FieldVector generics;

    StringVector members;
    std::vector<EnumDefinition> nested_enums;
};

struct FunctionDefinition
{
    std::string name;
    FieldVector generics;

    FieldVector parameters;
    TypeAnnotation_ptr return_type;

    Block block;

    bool is_pure;
    bool is_shared;
};

using FunctionDefinitionVector = std::vector<FunctionDefinition>;

struct OperatorDefinition
{
    std::string name;
    FieldVector generics;

    TokenType op_type;
    TokenType fixity;

    FieldVector operands;
    TypeAnnotation_ptr return_type;

    Block block;
};

struct TypeDefinition
{
    std::string name;

    enum class Kind
    {
        CLASS,
        TRAIT,
        PRIMITIVE
    } kind;

    FieldVector generics;
    FieldVector fields;
    FunctionDefinitionVector methods;

    TypeAnnotationVector traits;
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
    StringVector excluded_names;
};

// --- Variant  ---

using StatementVariant = std::variant<
    std::monostate,

    Import,
    ExpressionStatement,

    TypeAliasDefinition,
    EnumDefinition,
    FunctionDefinition,
    OperatorDefinition,
    TypeDefinition,

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
