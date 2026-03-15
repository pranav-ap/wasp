#pragma once

#include "Expression.h"
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

namespace Wasp {

struct Statement;
struct ExpressionStatement;

using Statement_ptr = std::shared_ptr<Statement>;
using Block = std::vector<Statement_ptr>;

struct ExpressionStatement {
    Expression_ptr expression;

    ExpressionStatement() = default;
    explicit ExpressionStatement(Expression_ptr expr) : expression(std::move(expr)) {}
};

// Definitions

struct Definition : public Resolvable {};

struct VariableDefinition : public Definition {
    Expression_ptr expression;
    bool is_mutable;

    VariableDefinition() = default;

    VariableDefinition(Expression_ptr expression, bool is_mutable)
        : expression(std::move(expression)), is_mutable(is_mutable) {};
};

struct AliasDefinition : public Definition {
    std::string name;
    TypeAnnotation_ptr ref_type;

    AliasDefinition() = default;

    AliasDefinition(std::string name, TypeAnnotation_ptr ref_type)
        : name(name), ref_type(ref_type) {};
};

struct EnumDefinition : public Definition {
    std::string name;
    std::map<std::string, int> members;

    EnumDefinition() = default;

    EnumDefinition(std::string name, std::vector<std::string> member_list) : name(std::move(name)) {
        int index = 0;
        for (const auto& member : member_list) {
            members.emplace(member, index++);
        }
    }
};

struct FunctionDefinition : public Definition {
    std::string name;
    std::vector<std::pair<std::string, TypeAnnotation_ptr>> parameters;
    TypeAnnotation_ptr return_type;
    Block body;

    FunctionDefinition() = default;

    FunctionDefinition(
        std::string name,
        std::vector<std::pair<std::string, TypeAnnotation_ptr>> params,
        TypeAnnotation_ptr ret_type,
        Block body
    )
        : name(std::move(name)), parameters(std::move(params)), return_type(std::move(ret_type)),
          body(std::move(body)) {};
};

struct AnnotationDefinition : public Definition {
    std::string name;
    ExpressionVector anno_values;

    AnnotationDefinition() = default;

    AnnotationDefinition(std::string name, ExpressionVector anno_values)
        : name(std::move(name)), anno_values(std::move(anno_values)) {};
};

struct ClassDefinition : public Definition {
    std::string name;
    std::map<std::string, TypeAnnotation_ptr> members;
    std::vector<std::string> traits;

    ClassDefinition() = default;

    ClassDefinition(
        std::string name,
        std::map<std::string, TypeAnnotation_ptr> members,
        std::vector<std::string> traits
    )
        : name(name), members(std::move(members)), traits(std::move(traits)) {};
};

struct TraitDefinition : public Definition {
    std::string name;
    std::map<std::string, TypeAnnotation_ptr> members;

    TraitDefinition() = default;

    TraitDefinition(std::string name, std::map<std::string, TypeAnnotation_ptr> members)
        : name(name), members(std::move(members)) {};
};

struct ImplDefinition : public Definition {
    std::string class_name;
    std::optional<std::string> trait_name;
    std::vector<Statement_ptr> methods;

    ImplDefinition() = default;

    ImplDefinition(
        std::string class_name,
        std::optional<std::string> trait_name,
        std::vector<Statement_ptr> methods
    )
        : class_name(std::move(class_name)), trait_name(std::move(trait_name)),
          methods(std::move(methods)) {}
};

// Branching

struct Branch {
    Block body;
};

struct IfBranch : Branch {
    Expression_ptr test;
    std::optional<Statement_ptr> alternative;

    IfBranch() = default;

    IfBranch(Expression_ptr test, Block body)
        : Branch(body), test(test), alternative(std::nullopt) {};

    IfBranch(Expression_ptr test, Block body, Statement_ptr alternative)
        : Branch(body), test(test), alternative(std::make_optional(alternative)) {};
};

struct ElseBranch : Branch {
    ElseBranch() = default;
    ElseBranch(Block body) : Branch(body) {};
};

// Looping

struct Loop {
    Block body;
};

struct SimpleLoop : public Loop {
    Expression_ptr condition;
    TokenType style;

    SimpleLoop() = default;

    SimpleLoop(Block body, Expression_ptr condition, TokenType style)
        : Loop(body), condition(std::move(condition)), style(style) {};
};

struct ForInLoop : public Loop {
    bool lhs_is_mutable;
    Expression_ptr lhs;
    Expression_ptr iterable_expression;

    ForInLoop() = default;

    ForInLoop(
        Block body, Expression_ptr lhs, Expression_ptr iterable_expression, bool lhs_is_mutable
    )
        : Loop(body), lhs_is_mutable(lhs_is_mutable), lhs(std::move(lhs)),
          iterable_expression(std::move(iterable_expression)) {};
};

// Other

struct Pass {};

struct Return {
    std::optional<Expression_ptr> expression;

    Return() : expression(std::nullopt) {};

    Return(Expression_ptr expression) : expression(std::make_optional(std::move(expression))) {};
};

// Loop Controls

struct LoopControl {
    TokenType type;
    std::string label;

    LoopControl() = default;

    LoopControl(TokenType type, std::string label = "") : type(type), label(label) {}
};

// Imports

struct AbstractImport {
    // std::nullopt means it's a 3rd party lib (like 'math3d')
    // Otherwise it holds the keyword: my, our, pkg, top, or up
    std::optional<TokenType> access_token_type;

    // ["engine", "fuel"]
    std::vector<std::string> path;

    std::filesystem::path resolved_path;

    AbstractImport() = default;

    AbstractImport(std::optional<TokenType> access_token_type, std::vector<std::string> path)
        : access_token_type(access_token_type), path(std::move(path)) {}

    virtual ~AbstractImport() = default;
};

// import top.engine.fuel as f
struct SimpleImport : public AbstractImport, Resolvable {
    std::optional<std::string> alias;

    SimpleImport() = default;

    SimpleImport(
        std::optional<TokenType> access_token_type,
        std::vector<std::string> path,
        std::optional<std::string> alias = std::nullopt
    )
        : AbstractImport(access_token_type, std::move(path)), alias(std::move(alias)) {}
};

// Tank as FuelTank
struct ImportedSymbol {
    std::string name;
    std::optional<std::string> alias;
};

// from top.engine import Tank, Pump
struct FromImport : public AbstractImport, Resolvable {
    std::vector<ImportedSymbol> symbols;

    FromImport() = default;

    FromImport(
        std::optional<TokenType> access_token_type,
        std::vector<std::string> path,
        std::vector<ImportedSymbol> symbols
    )
        : AbstractImport(access_token_type, std::move(path)), symbols(std::move(symbols)) {}
};

// Statement Variant

struct Statement {
    using StatementData = std::variant<
        std::monostate,
        ExpressionStatement,

        VariableDefinition,
        AliasDefinition,
        EnumDefinition,
        FunctionDefinition,
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

    StatementData data;

    int statement_number; // For error reporting and debugging

    Statement() = default;

    template <typename T> Statement(T&& val) : data(std::forward<T>(val)) {}

    template <typename T> bool is() const { return std::holds_alternative<T>(data); }

    template <typename T> const T& as() const { return std::get<T>(data); }
    template <typename T> const T* try_as() const { return std::get_if<T>(&data); }

    template <typename T> T& as() { return std::get<T>(data); }
    template <typename T> T* try_as() { return std::get_if<T>(&data); }
};

template <typename T> Statement_ptr make_statement(T&& data, int current_line_number) {
    auto stmt = std::make_shared<Statement>(std::forward<T>(data));
    stmt->statement_number = current_line_number;
    return stmt;
}

} // namespace Wasp