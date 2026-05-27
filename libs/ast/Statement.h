#pragma once

#include "AST.h"
#include "Resolvable.h"
#include "Token.h"
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

using FieldDefinitionVector = std::vector<FieldDefinition>;

struct Templatable
{
    FieldDefinitionVector template_params;

    Templatable() = default;

    explicit Templatable(FieldDefinitionVector template_params)
        : template_params(std::move(template_params))
    {
    }
};

// --- Callables ---

struct Field
{
    std::string name;
    TypeAnnotation_ptr type;
    bool is_static;

    Field() = default;

    Field(std::string name, TypeAnnotation_ptr type, bool is_static = false)
        : name(std::move(name)), type(std::move(type)), is_static(is_static)
    {
    }
};

struct AbstractCallable : public Definition, public Templatable
{
    std::vector<Field> parameters;
    std::vector<std::shared_ptr<Symbol>> parameter_symbols;
    TypeAnnotation_ptr return_type;

    StatementVector body;

    std::shared_ptr<Symbol> context_symbol = nullptr;
    std::shared_ptr<Symbol> group_symbol;

    bool is_pure = false;

    AbstractCallable() = default;

    AbstractCallable(
        std::string name,
        std::vector<Field> params,
        TypeAnnotation_ptr ret,
        StatementVector body,
        bool is_pure = false,
        FieldDefinitionVector template_params = {}
    )
        : Definition(std::move(name)), Templatable(std::move(template_params)),
          parameters(std::move(params)), return_type(std::move(ret)),
          body(std::move(body)), is_pure(is_pure)
    {
    }
};

struct FunctionDefinition : public AbstractCallable
{
    using AbstractCallable::AbstractCallable;
};

struct MethodDefinition : public AbstractCallable
{
    bool is_static = false;

    MethodDefinition() = default;

    MethodDefinition(
        std::string name,
        std::vector<Field> params,
        TypeAnnotation_ptr ret,
        StatementVector body,
        bool is_pure = false,
        bool is_static = false,
        FieldDefinitionVector template_params = {}
    )
        : AbstractCallable(
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

struct OperatorDefinition : public AbstractCallable
{
    TokenType op_type;
    TokenType fixity;

    OperatorDefinition() = default;

    OperatorDefinition(
        TokenType fix,
        TokenType op,
        std::string mangled,
        std::vector<Field> params,
        TypeAnnotation_ptr ret,
        StatementVector body,
        FieldDefinitionVector template_params = {}
    )
        : AbstractCallable(
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

    AbstractOopsDefinition() = default;

    AbstractOopsDefinition(
        std::string name,
        TypeAnnotationVector traits,
        StatementVector members,
        FieldDefinitionVector template_params = {}
    )
        : Definition(std::move(name)), Templatable(std::move(template_params)),
          traits(std::move(traits)), members(std::move(members))
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
        FieldDefinitionVector gen = {}
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
    std::optional<Statement_ptr> alternative;

    IfBranch() = default;

    IfBranch(
        Expression_ptr test,
        StatementVector body,
        Statement_ptr alt = nullptr
    )
        : Branch{std::move(body)}, test(std::move(test)),
          alternative(alt ? std::make_optional(alt) : std::nullopt)
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
    std::shared_ptr<Symbol> iterator_symbol;

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
    // REQUIRED, or NATIVE
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
    std::optional<TokenType> access_modifier;
    int access_argument = 1;
    StringVector path;
    std::filesystem::path absolute_path;
    std::optional<std::string> module_alias;
    bool expose_all = false;
    std::vector<ImportAsPair> exposed_symbols;
    StringVector excluded_symbols;

    Import() = default;

    Import(
        std::optional<TokenType> mod,
        int arg,
        StringVector p,
        std::optional<std::string> alias = std::nullopt,
        bool all = false,
        std::vector<ImportAsPair> syms = {},
        StringVector ex = {}
    )
        : access_modifier(mod), access_argument(arg), path(std::move(p)),
          module_alias(std::move(alias)), expose_all(all),
          exposed_symbols(std::move(syms)), excluded_symbols(std::move(ex))
    {
    }
};

// ============================================================================
// Splitter
// ============================================================================

struct Splitter
{
    std::vector<Statement_ptr> statements;

    Splitter() = default;

    explicit Splitter(std::vector<Statement_ptr> statements)
        : statements(std::move(statements))
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

    FieldDefinition,

    ClassDefinition,
    TraitDefinition,

    Import,

    IfBranch,
    ElseBranch,

    SimpleLoop,
    ForInLoop,
    LoopControl,

    Placeholder,
    Return,

    Splitter>;

struct Statement : public AstNode<StatementVariant>
{
    using AstNode::AstNode;
};

template <typename T> inline Statement_ptr make_statement(T&& data)
{
    return std::make_shared<Statement>(std::forward<T>(data));
}

inline std::string get_operator_name(TokenType fixity, TokenType op_type)
{
    std::string fix;

    if (fixity == TokenType::INFIX)
    {
        fix = "infix_";
    }
    if (fixity == TokenType::PREFIX)
    {
        fix = "prefix_";
    }
    if (fixity == TokenType::POSTFIX)
    {
        fix = "postfix_";
    }

    return fix + to_string(op_type);
}

} // namespace Wasp
