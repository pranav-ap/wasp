#pragma once

#include "AST.h"
#include "Resolvable.h"
#include "Token.h"
#include "TypeAnnotation.h"

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
};

struct VariableDefinition : public Definition
{
    Expression_ptr expression;
    bool is_mutable;

    VariableDefinition() = default;

    VariableDefinition(Expression_ptr expression, bool is_mutable)
        : expression(std::move(expression)), is_mutable(is_mutable) {};
};

struct AliasDefinition : public Definition
{
    std::string name;
    TypeAnnotation_ptr ref_type;

    AliasDefinition() = default;

    AliasDefinition(std::string name, TypeAnnotation_ptr ref_type)
        : name(name), ref_type(ref_type) {};
};

struct EnumDefinition : public Definition
{
    std::string name;
    std::map<std::string, int> members;

    EnumDefinition() = default;

    EnumDefinition(std::string name, StringVector member_list) : name(std::move(name))
    {
        int index = 0;
        for (const auto& member : member_list)
        {
            members.emplace(member, index++);
        }
    }
};

struct AbstractFunctionDefinition : public Definition
{
    std::string name;

    std::vector<std::pair<std::string, TypeAnnotation_ptr>> parameters;
    std::vector<std::shared_ptr<Symbol>> parameter_symbols;

    TypeAnnotation_ptr return_type;
    StatementVector body;

    std::shared_ptr<Symbol> group_symbol;

    AbstractFunctionDefinition() = default;

    AbstractFunctionDefinition(
        std::string name,
        std::vector<std::pair<std::string, TypeAnnotation_ptr>> parameters,
        TypeAnnotation_ptr ret_type,
        StatementVector body
    )
        : name(std::move(name)), parameters(std::move(parameters)),
          return_type(std::move(ret_type)), body(std::move(body))
    {
    }
};

struct FunctionDefinition : public AbstractFunctionDefinition
{
    using AbstractFunctionDefinition::AbstractFunctionDefinition;
};

struct MyMethodDefinition : public AbstractFunctionDefinition
{
    using AbstractFunctionDefinition::AbstractFunctionDefinition;
};

struct OurMethodDefinition : public AbstractFunctionDefinition
{
    using AbstractFunctionDefinition::AbstractFunctionDefinition;
};

struct AnnotationDefinition : public Definition
{
    std::string name;
    ExpressionVector anno_values;

    AnnotationDefinition() = default;

    AnnotationDefinition(std::string name, ExpressionVector anno_values)
        : name(std::move(name)), anno_values(std::move(anno_values)) {};
};

struct MemberedDefinition : public Definition
{
    std::string name;
    StringVector traits;
    std::map<std::string, MemberInfo> members;

    MemberedDefinition() = default;

    MemberedDefinition(
        std::string name,
        StringVector traits,
        std::map<std::string, MemberInfo> members
    )
        : name(std::move(name)), traits(std::move(traits)), members(std::move(members))
    {
    }
};

struct ClassDefinition : public MemberedDefinition
{
    ClassDefinition() = default;

    ClassDefinition(
        std::string name,
        StringVector traits,
        std::map<std::string, MemberInfo> members
    )
        : MemberedDefinition(std::move(name), std::move(traits), std::move(members))
    {
    }
};

struct TraitDefinition : public MemberedDefinition
{
    TraitDefinition() = default;

    TraitDefinition(
        std::string name,
        StringVector traits,
        std::map<std::string, MemberInfo> members
    )
        : MemberedDefinition(std::move(name), std::move(traits), std::move(members))
    {
    }
};

struct ImplDefinition : public Definition
{
    std::string class_name;
    StatementVector methods;

    ImplDefinition() = default;

    ImplDefinition(std::string class_name, StatementVector methods)
        : class_name(std::move(class_name)), methods(std::move(methods))
    {
    }
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
struct ImportedSymbol
{
    std::string name;
    std::optional<std::string> alias;

    std::vector<std::shared_ptr<Symbol>> resolved_symbols;

    ImportedSymbol() = default;

    ImportedSymbol(std::string name, std::optional<std::string> alias = std::nullopt)
        : name(std::move(name)), alias(std::move(alias))
    {
    }
};

// from top.engine import Tank, Pump
struct FromImport : public AbstractImport
{
    std::vector<ImportedSymbol> symbols;

    FromImport() = default;

    FromImport(
        std::optional<TokenType> access_token_type,
        StringVector path,
        std::vector<ImportedSymbol> symbols
    )
        : AbstractImport(access_token_type, std::move(path)), symbols(std::move(symbols))
    {
    }
};

// Statement Variant

using StatementVariant = std::variant<
    std::monostate,
    ExpressionStatement,

    VariableDefinition,
    AliasDefinition,
    EnumDefinition,
    FunctionDefinition,
    MyMethodDefinition,
    OurMethodDefinition,
    ClassDefinition,
    TraitDefinition,
    ImplDefinition,

    AnnotationDefinition,

    SimpleImport,
    FromImport,

    IfBranch,
    ElseBranch,
    SimpleLoop,
    ForInLoop,
    LoopControl,

    Pass,
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
