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

struct ExpressionStatement
{
    Expression_ptr expression;

    ExpressionStatement() = default;

    explicit ExpressionStatement(Expression_ptr expr)
        : expression(std::move(expr))
    {
    }
};

struct Definition
{
    std::string name;

    Definition() = default;
    virtual ~Definition() = default;

    explicit Definition(std::string name) : name(std::move(name))
    {
    }
};

struct Field
{
    std::string name;
    TypeAnnotation_ptr type;
    bool is_variadic;

    Field() = default;

    Field(std::string name, TypeAnnotation_ptr type, bool is_variadic = false)
        : name(std::move(name)), type(std::move(type)), is_variadic(is_variadic)
    {
    }
};

using FieldVector = std::vector<Field>;

struct Templatable
{
    FieldVector template_params;

    Templatable() = default;

    explicit Templatable(FieldVector template_params)
        : template_params(std::move(template_params))
    {
    }
};

struct Block
{
    StatementVector statements;
};

// --- Callables ---

struct CallableDefinition : public Definition, public Templatable
{
    FieldVector parameters;
    TypeAnnotation_ptr return_type;

    StatementVector body;

    bool is_pure = false;

    CallableDefinition() = default;

    CallableDefinition(
        std::string name,
        FieldVector params,
        TypeAnnotation_ptr ret,
        StatementVector body,
        bool is_pure = false,
        FieldVector template_params = {}
    )
        : Definition(std::move(name)), Templatable(std::move(template_params)),
          parameters(std::move(params)), return_type(std::move(ret)),
          body(std::move(body)), is_pure(is_pure)
    {
    }
};

struct FunctionDefinition : public CallableDefinition
{
    using CallableDefinition::CallableDefinition;
};

struct MethodDefinition : public CallableDefinition
{
    bool is_static = false;

    MethodDefinition() = default;

    MethodDefinition(
        std::string name,
        FieldVector params,
        TypeAnnotation_ptr ret,
        StatementVector body,
        bool is_pure = false,
        bool is_static = false,
        FieldVector template_params = {}
    )
        : CallableDefinition(
              std::move(name),
              std::move(params),
              std::move(ret),
              std::move(body),
              is_pure,
              std::move(template_params)
          ),
          is_static(is_static)
    {
    }
};

struct OperatorDefinition : public CallableDefinition
{
    TokenType op_type;
    TokenType fixity;

    OperatorDefinition() = default;

    OperatorDefinition(
        TokenType fix,
        TokenType op,
        std::string mangled,
        FieldVector params,
        TypeAnnotation_ptr ret,
        StatementVector body,
        FieldVector template_params = {}
    )
        : CallableDefinition(
              std::move(mangled),
              std::move(params),
              std::move(ret),
              std::move(body),
              true,
              std::move(template_params)
          ),
          op_type(op), fixity(fix)
    {
    }
};

// --- OOP ---

struct AbstractOopsDefinition : public Definition, public Templatable
{
    TypeAnnotationVector traits;
    StatementVector members;
    bool is_primitive;

    AbstractOopsDefinition() = default;

    AbstractOopsDefinition(
        std::string name,
        TypeAnnotationVector traits,
        StatementVector members,
        FieldVector template_params = {}
    )
        : Definition(std::move(name)), Templatable(std::move(template_params)),
          traits(std::move(traits)), members(std::move(members)),
          is_primitive(
              !name.empty() && std::islower(static_cast<unsigned char>(name[0]))
          )
    {
    }
};

struct ClassDefinition : public AbstractOopsDefinition
{
    using AbstractOopsDefinition::AbstractOopsDefinition;
};

struct TraitDefinition : public AbstractOopsDefinition
{
    using AbstractOopsDefinition::AbstractOopsDefinition;
};

struct TypeAliasDefinition : public Definition, public Templatable
{
    TypeAnnotation_ptr ref_type;

    TypeAliasDefinition() = default;

    TypeAliasDefinition(
        std::string name,
        TypeAnnotation_ptr ref,
        FieldVector gen = {}
    )
        : Definition(std::move(name)), Templatable(std::move(gen)),
          ref_type(std::move(ref))
    {
    }
};

struct EnumDefinition : public Definition
{
    StringVector members;
    std::vector<EnumDefinition> nested_enums;

    EnumDefinition() = default;

    EnumDefinition(std::string name, StringVector members, std::vector<EnumDefinition> nested = {})
        : Definition(std::move(name)), members(std::move(members)), nested_enums(std::move(nested))
    {
    }
};

// --- Control Flow ---

struct Branch
{
    StatementVector body;

    Branch() = default;

    explicit Branch(StatementVector body) : body(std::move(body))
    {
    }
};

struct IfBranch : Branch
{
    Expression_ptr test;
    Statement_ptr alternative;

    IfBranch() = default;

    IfBranch(
        Expression_ptr test,
        StatementVector body,
        Statement_ptr alternative = nullptr
    )
        : Branch(std::move(body)), test(std::move(test)),
          alternative(alternative)
    {
    }
};

struct ElseBranch : Branch
{
    using Branch::Branch;
};

struct SimpleLoop : public Branch
{
    Expression_ptr condition;
    TokenType style;

    SimpleLoop() = default;

    SimpleLoop(StatementVector body, Expression_ptr cond, TokenType style)
        : Branch{std::move(body)}, condition(std::move(cond)), style(style)
    {
    }
};

struct ForInLoop : public Branch
{
    bool lhs_is_mutable = false;
    Expression_ptr lhs;
    Expression_ptr iterable;

    ForInLoop() = default;

    ForInLoop(
        StatementVector body,
        Expression_ptr lhs,
        Expression_ptr iter,
        bool mut
    )
        : Branch{std::move(body)}, lhs_is_mutable(mut), lhs(std::move(lhs)),
          iterable(std::move(iter))
    {
    }
};

// --- Others ---

struct Placeholder
{
    // REQUIRED, NATIVE or PASS
    TokenType type;

    Placeholder() = default;

    explicit Placeholder(TokenType type) : type(type)
    {
    }
};

struct Return
{
    std::optional<Expression_ptr> expression;

    Return() = default;

    explicit Return(Expression_ptr expr) : expression(std::move(expr))
    {
    }
};

struct LoopControl
{
    TokenType type;
    std::string label;

    LoopControl() = default;

    explicit LoopControl(TokenType type, std::string label = "")
        : type(type), label(std::move(label))
    {
    }
};

// --- Imports ---

struct ImportAsPair
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

struct Import
{
    std::optional<TokenType> access_modifier;
    int access_argument = 1;
    StringVector path;
    std::filesystem::path absolute_path;
    std::optional<std::string> module_alias;
    bool expose_all = false;
    std::vector<ImportAsPair> exposed_names;
    StringVector excluded_names;

    Import() = default;

    Import(
        std::optional<TokenType> access_modifier,
        int access_argument,
        StringVector path,
        std::optional<std::string> module_alias = std::nullopt,
        bool all = false,
        std::vector<ImportAsPair> exposed_names = {},
        StringVector excluded_names = {}
    )
        : access_modifier(access_modifier), access_argument(access_argument),
          path(std::move(path)), module_alias(std::move(module_alias)),
          expose_all(all), exposed_names(std::move(exposed_names)),
          excluded_names(std::move(excluded_names))
    {
    }
};

// --- Variant & Utils ---

using StatementVariant = std::variant<
    std::monostate,

    ExpressionStatement,

    TypeAliasDefinition,
    EnumDefinition,

    FunctionDefinition,
    MethodDefinition,
    OperatorDefinition,

    Field,
    Block,

    ClassDefinition,
    TraitDefinition,

    Import,

    IfBranch,
    ElseBranch,

    SimpleLoop,
    ForInLoop,
    LoopControl,

    Placeholder,
    Return>;

struct Statement : public AstNode<StatementVariant>
{
    using AstNode::AstNode;
};

template <typename T> inline Statement_ptr make_statement(T&& data)
{
    return std::make_shared<Statement>(std::forward<T>(data));
}

} // namespace Wasp
