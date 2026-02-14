#pragma once

#include "Expression.h"
#include <memory>
#include <variant>
#include <utility>

namespace Wasp {

struct Statement;
struct ExpressionStatement;

using Statement_ptr = std::shared_ptr<Statement>;
using Block = std::vector<Statement_ptr>;

struct ExpressionStatement
{
    Expression_ptr expression;

    explicit ExpressionStatement(Expression_ptr expr)
        : expression(std::move(expr)) {}
};

// Definitions

struct Definition  
{
};


struct VariableDefinition : public Definition {
	Expression_ptr expression;
    bool is_mutable;

	VariableDefinition(Expression_ptr expression, bool is_mutable)
		: expression(std::move(expression)), is_mutable(is_mutable) {};
};


struct AliasDefinition : public Definition {
	std::string name;
	TypeAnnotation_ptr ref_type;

	AliasDefinition(std::string name, TypeAnnotation_ptr ref_type)
		: name(name), ref_type(ref_type) {};
};


// Branching

struct Branch
{
    Block body;
};

struct IfBranch : Branch
{
	Expression_ptr test;
	std::optional<Statement_ptr> alternative;

	IfBranch(Expression_ptr test, Block body)
		: Branch(body), test(test), alternative(std::nullopt) {};

	IfBranch(Expression_ptr test, Block body, Statement_ptr alternative)
		: Branch(body), test(test), alternative(std::make_optional(alternative)) {};
};

struct ElseBranch : Branch
{
	ElseBranch(Block body) : Branch(body) {};
};

// Other 

struct Pass {};

struct Statement
{
    using StatementData = std::variant<
        std::monostate,
        ExpressionStatement,
        
	    VariableDefinition, AliasDefinition,
        IfBranch, ElseBranch,

		Pass 
    >;

    StatementData data;
};

}