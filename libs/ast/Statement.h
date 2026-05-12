#pragma once

#include "AST.h"
#include "Resolvable.h"
#include "Token.h"

#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace Wasp
{

struct ExpressionStatement;

struct ExpressionStatement
{
    Expression_ptr expression;

    ExpressionStatement() = default;

    explicit ExpressionStatement(Expression_ptr expr)
        : expression(std::move(expr))
    {
    }
};

// --- Core Structural Components ---

struct Definition : public Resolvable
{
    std::string name;

    Definition() = default;
    virtual ~Definition() = default;

    explicit Definition(std::string name) : name(std::move(name))
    {
    }
};

struct FieldDefinition : public Definition
{
    TypeAnnotation_ptr type;

    FieldDefinition() = default;

    FieldDefinition(std::string name, TypeAnnotation_ptr type)
        : Definition(std::move(name)), type(std::move(type))
    {
    }
};

struct Templatable
{
    std::vector<FieldDefinition> generics;

    Templatable() = default;

    explicit Templatable(std::vector<FieldDefinition> generics)
        : generics(std::move(generics))
    {
    }
};

// --- Function Definitions ---

struct AbstractFunctionDefinition : public Definition, public Templatable
{
    std::vector<std::pair<std::string, TypeAnnotation_ptr>> parameters;
    std::vector<std::shared_ptr<Symbol>> parameter_symbols;

    TypeAnnotation_ptr return_type;
    StatementVector body;

    std::shared_ptr<Symbol> group_symbol;

    bool is_pure = false;

    AbstractFunctionDefinition() = default;

    AbstractFunctionDefinition(
        std::string name,
        std::vector<std::pair<std::string, TypeAnnotation_ptr>> parameters,
        TypeAnnotation_ptr return_type,
        StatementVector body,
        bool is_pure = false,
        std::vector<FieldDefinition> generics = {}
    )
        : Definition(std::move(name)), Templatable(std::move(generics)),
          parameters(std::move(parameters)),
          return_type(std::move(return_type)), body(std::move(body)),
          is_pure(is_pure)
    {
    }
};

struct FunctionDefinition : public AbstractFunctionDefinition
{
    bool is_method = false;
    bool is_static = false;

    FunctionDefinition() = default;

    FunctionDefinition(
        std::string name,
        std::vector<std::pair<std::string, TypeAnnotation_ptr>> parameters,
        TypeAnnotation_ptr return_type,
        StatementVector body,
        bool is_pure = false,
        bool is_method = false,
        bool is_static = false,
        std::vector<FieldDefinition> generics = {}
    )
        : AbstractFunctionDefinition(
              std::move(name),
              std::move(parameters),
              std::move(return_type),
              std::move(body),
              is_pure,
              std::move(generics)
          ),
          is_method(is_method), is_static(is_static)
    {
    }
};

struct OperatorDefinition : public AbstractFunctionDefinition
{
    TokenType operator_token_type;
    TokenType fixity;

    OperatorDefinition() = default;

    OperatorDefinition(
        TokenType fixity,
        TokenType operator_token_type,
        std::vector<std::pair<std::string, TypeAnnotation_ptr>> parameters,
        TypeAnnotation_ptr return_type,
        StatementVector body,
        std::vector<FieldDefinition> generics = {}
    )
        : AbstractFunctionDefinition(
              to_string(operator_token_type),
              std::move(parameters),
              std::move(return_type),
              std::move(body),
              true, // Operators are natively pure
              std::move(generics)
          ),
          operator_token_type(operator_token_type), fixity(fixity)
    {
    }
};

// --- Object Oriented Definitions ---

struct AbstractOOPDefinition : public Definition, public Templatable
{
    StringVector traits;
    StatementVector members;

    AbstractOOPDefinition() = default;

    AbstractOOPDefinition(
        std::string name,
        StringVector traits,
        StatementVector members,
        std::vector<FieldDefinition> generics = {}
    )
        : Definition(std::move(name)), Templatable(std::move(generics)),
          traits(std::move(traits)), members(std::move(members))
    {
    }
};

struct ClassDefinition : public AbstractOOPDefinition
{
    using AbstractOOPDefinition::AbstractOOPDefinition;
};

struct TraitDefinition : public AbstractOOPDefinition
{
    using AbstractOOPDefinition::AbstractOOPDefinition;
};

struct TypeAliasDefinition : public Definition, public Templatable
{
    TypeAnnotation_ptr ref_type;

    TypeAliasDefinition() = default;

    TypeAliasDefinition(
        std::string name,
        TypeAnnotation_ptr ref_type,
        std::vector<FieldDefinition> generics = {}
    )
        : Definition(std::move(name)), Templatable(std::move(generics)),
          ref_type(std::move(ref_type))
    {
    }
};

struct EnumDefinition : public Definition
{
    std::map<std::string, int> members;
    std::vector<EnumDefinition> nested_enums;

    EnumDefinition() = default;

    EnumDefinition(
        std::string name,
        const StringVector& member_list,
        std::vector<EnumDefinition> nested = {}
    )
        : Definition(std::move(name)), nested_enums(std::move(nested))
    {
        int index = 0;
        for (const auto& member : member_list)
        {
            members.emplace(member, index++);
        }
    }
};

// --- Control Flow ---

struct Branch
{
    StatementVector body;
};

struct IfBranch : Branch
{
    Expression_ptr test;
    std::optional<Statement_ptr> alternative;

    IfBranch() = default;

    IfBranch(Expression_ptr test, StatementVector body)
        : Branch(body), test(test), alternative(std::nullopt) {};

    IfBranch(
        Expression_ptr test,
        StatementVector body,
        Statement_ptr alternative
    )
        : Branch(body), test(test),
          alternative(std::make_optional(alternative)) {};
};

struct ElseBranch : Branch
{
    ElseBranch() = default;
    explicit ElseBranch(StatementVector body) : Branch(body) {};
};

struct Loop
{
    StatementVector body;
};

struct SimpleLoop : public Loop
{
    Expression_ptr condition;
    TokenType style;

    SimpleLoop() = default;

    SimpleLoop(StatementVector body, Expression_ptr condition, TokenType style)
        : Loop(body), condition(std::move(condition)), style(style) {};
};

struct ForInLoop : public Loop
{
    bool lhs_is_mutable;
    Expression_ptr lhs;
    Expression_ptr iterable_expression;

    std::shared_ptr<Symbol> iterator_symbol;

    ForInLoop() = default;

    ForInLoop(
        StatementVector body,
        Expression_ptr lhs,
        Expression_ptr iterable_expression,
        bool lhs_is_mutable
    )
        : Loop(body), lhs_is_mutable(lhs_is_mutable), lhs(std::move(lhs)),
          iterable_expression(std::move(iterable_expression)) {};
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

    Return() : expression(std::nullopt) {};

    explicit Return(Expression_ptr expression)
        : expression(std::make_optional(std::move(expression))) {};
};

struct LoopControl
{
    TokenType type;
    std::string label;

    LoopControl() = default;

    explicit LoopControl(TokenType type, std::string label = "")
        : type(type), label(label)
    {
    }
};

// --- Submodules & Modules Imports ---

struct ImportAsPair : public Resolvable
{
    std::string name;
    std::optional<std::string> alias;

    ImportAsPair() = default;

    explicit ImportAsPair(
        std::string name,
        std::optional<std::string> alias = std::nullopt
    )
        : name(std::move(name)), alias(std::move(alias))
    {
    }
};

struct Import : public Resolvable
{
    std::optional<TokenType> access_modifier = std::nullopt;
    int access_argument = 1;

    StringVector path;
    std::filesystem::path absolute_path;

    std::optional<std::string> module_alias = std::nullopt;

    bool expose_all = false;
    std::vector<ImportAsPair> exposed_symbols;
    StringVector excluded_symbols;

    Import() = default;

    Import(
        std::optional<TokenType> access_modifier,
        int access_argument,
        StringVector path,
        std::optional<std::string> module_alias = std::nullopt,
        bool expose_all = false,
        std::vector<ImportAsPair> exposed_symbols = {},
        StringVector excluded_symbols = {}
    )
        : access_modifier(access_modifier), access_argument(access_argument),
          path(std::move(path)), module_alias(std::move(module_alias)),
          expose_all(expose_all), exposed_symbols(std::move(exposed_symbols)),
          excluded_symbols(std::move(excluded_symbols))
    {
    }
};

using StatementVariant = std::variant<
    std::monostate,
    ExpressionStatement,

    TypeAliasDefinition,
    EnumDefinition,

    FunctionDefinition,
    OperatorDefinition,

    FieldDefinition,
    ClassDefinition,
    TraitDefinition,

    Import,

    IfBranch,
    ElseBranch,
    SimpleLoop,
    ForInLoop,
    LoopControl,

    Pass,
    Native,
    Required,

    Return>;

struct Statement : public AstNode<StatementVariant>
{
    using AstNode::AstNode;

    Token start_token;
    Token end_token;
};

template <typename T> inline Statement_ptr make_statement(T&& data)
{
    return std::make_shared<Statement>(std::forward<T>(data));
}

template <typename T>
inline Statement_ptr make_statement(T&& data, int statement_number)
{
    auto stmt = std::make_shared<Statement>(std::forward<T>(data));
    stmt->statement_number = statement_number;
    return stmt;
}

} // namespace Wasp
