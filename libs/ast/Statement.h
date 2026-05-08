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
    explicit ExpressionStatement(Expression_ptr expr) : expression(std::move(expr))
    {
    }
};

// Definitions

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

struct AbstractFunctionDefinition : public Definition
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
        bool is_pure = false
    )
        : Definition(std::move(name)), parameters(std::move(parameters)),
          return_type(std::move(return_type)), body(std::move(body)),
          is_pure(is_pure)
    {
    }
};

// Standalone functions remain Templatable
struct FunctionDefinition : public AbstractFunctionDefinition, public Templatable
{
    FunctionDefinition() = default;

    FunctionDefinition(
        std::string name,
        std::vector<std::pair<std::string, TypeAnnotation_ptr>> parameters,
        TypeAnnotation_ptr return_type,
        StatementVector body,
        bool is_pure = false,
        std::vector<FieldDefinition> generics = {}
    )
        : AbstractFunctionDefinition(
              std::move(name),
              std::move(parameters),
              std::move(return_type),
              std::move(body),
              is_pure
          ),
          Templatable(std::move(generics))
    {
    }
};

struct MethodDefinition : public AbstractFunctionDefinition
{
    bool is_static = false;

    MethodDefinition() = default;

    MethodDefinition(
        std::string name,
        std::vector<std::pair<std::string, TypeAnnotation_ptr>> parameters,
        TypeAnnotation_ptr return_type,
        StatementVector body,
        bool is_pure = false,
        bool is_static = false
    )
        : AbstractFunctionDefinition(
              std::move(name),
              std::move(parameters),
              std::move(return_type),
              std::move(body),
              is_pure
          ),
          is_static(is_static)
    {
    }
};

struct ClassDefinition : public Definition, public Templatable
{
    StringVector traits;
    StatementVector members;

    ClassDefinition() = default;

    ClassDefinition(
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

struct TraitDefinition : public Definition, public Templatable
{
    StringVector traits;
    StatementVector members;

    TraitDefinition() = default;

    TraitDefinition(
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

struct VariableDefinition : public Definition
{
    Expression_ptr expression;
    bool is_mutable;

    VariableDefinition() = default;

    VariableDefinition(Expression_ptr expression, bool is_mutable)
        : expression(std::move(expression)), is_mutable(is_mutable) {};
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

struct AnnotationDefinition : public Definition
{
    ExpressionVector anno_values;

    AnnotationDefinition() = default;

    AnnotationDefinition(std::string name, ExpressionVector anno_values)
        : Definition(std::move(name)), anno_values(std::move(anno_values)) {};
};

// Branching

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

    IfBranch(Expression_ptr test, StatementVector body, Statement_ptr alternative)
        : Branch(body), test(test), alternative(std::make_optional(alternative)) {};
};

struct ElseBranch : Branch
{
    ElseBranch() = default;
    ElseBranch(StatementVector body) : Branch(body) {};
};

// Looping

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

// Other

struct Pass
{
};

struct Native
{
};

struct Return
{
    std::optional<Expression_ptr> expression;

    Return() : expression(std::nullopt) {};

    Return(Expression_ptr expression) : expression(std::make_optional(std::move(expression))) {};
};

// Loop Controls

struct LoopControl
{
    TokenType type;
    std::string label;

    LoopControl() = default;

    LoopControl(TokenType type, std::string label = "") : type(type), label(label)
    {
    }
};

// Imports

struct AbstractImport : public Resolvable
{
    // std::nullopt means it's a 3rd party lib (like 'math3d')
    // Otherwise it holds the keyword: my, our, pkg, top, or up
    std::optional<TokenType> access_token_type;

    // ["engine", "fuel"]
    StringVector path;

    std::filesystem::path absolute_path;

    AbstractImport() = default;

    AbstractImport(std::optional<TokenType> access_token_type, StringVector path)
        : access_token_type(access_token_type), path(std::move(path))
    {
    }

    virtual ~AbstractImport() = default;
};

// import top.engine.fuel as f
struct SimpleImport : public AbstractImport
{
    std::optional<std::string> alias;

    SimpleImport() = default;

    SimpleImport(
        std::optional<TokenType> access_token_type,
        StringVector path,
        std::optional<std::string> alias = std::nullopt
    )
        : AbstractImport(access_token_type, std::move(path)), alias(std::move(alias))
    {
    }
};

// Tank as FuelTank
struct ImportAsPair : public Resolvable
{
    std::string name;
    std::optional<std::string> alias;

    ImportAsPair() = default;

    ImportAsPair(std::string name, std::optional<std::string> alias = std::nullopt)
        : name(std::move(name)), alias(std::move(alias))
    {
    }
};

// from top.engine import Tank, Pump
struct FromImport : public AbstractImport
{
    std::vector<ImportAsPair> import_as_pairs;

    FromImport() = default;

    FromImport(
        std::optional<TokenType> access_token_type,
        StringVector path,
        std::vector<ImportAsPair> symbols
    )
        : AbstractImport(access_token_type, std::move(path)),
          import_as_pairs(std::move(symbols))
    {
    }
};

// Statement Variant

using StatementVariant = std::variant<
    std::monostate,
    ExpressionStatement,

    VariableDefinition,
    TypeAliasDefinition,
    EnumDefinition,

    FunctionDefinition,
    MethodDefinition,

    FieldDefinition,
    ClassDefinition,
    TraitDefinition,

    AnnotationDefinition,

    SimpleImport,
    FromImport,

    IfBranch,
    ElseBranch,
    SimpleLoop,
    ForInLoop,
    LoopControl,

    Pass,
    Native,
    Return>;

// STATEMENT

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

template <typename T> inline Statement_ptr make_statement(T&& data, int statement_number)
{
    auto stmt = std::make_shared<Statement>(std::forward<T>(data));
    stmt->statement_number = statement_number;
    return stmt;
}

} // namespace Wasp
