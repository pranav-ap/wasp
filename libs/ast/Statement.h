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

struct Definition
{
    std::string name;
    FieldVector generics;
};

struct TypeAliasDefinition : public Definition
{
    TypeAnnotation_ptr ref_type;
};

struct EnumDefinition : public Definition
{
    StringVector members;
    std::vector<EnumDefinition> nested_enums;
};

struct FunctionDefinition : public Definition
{
    FieldVector fields;
    TypeAnnotation_ptr return_type;

    Block block;

    bool is_pure = false;

    bool is_method = false;
    bool is_shared = false;
};

struct OperatorDefinition : public Definition
{
    TokenType op_type;
    TokenType fixity;

    FieldVector fields;
    TypeAnnotation_ptr return_type;

    Block body;
};

struct TypeDefinition : public Definition
{
    enum class Kind
    {
        Class,
        Trait,
        Primitive
    } kind;

    TypeAnnotationVector traits;
    StatementVector members;
};

// =============== Imports ===============

struct ImportAsPair
{
    std::string name;
    std::optional<std::string> alias;
};

struct Import
{
    std::optional<TokenType> access_modifier;
    int jumps = 1;

    StringVector path;

    std::optional<std::string> module_alias;
    bool expose_all = false;
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
