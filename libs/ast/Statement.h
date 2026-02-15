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

struct EnumDefinition : public Definition {
    std::string name;
    std::map<std::string, int> members;

    EnumDefinition(std::string name, std::vector<std::string> member_list) 
        : name(std::move(name)) {
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

    FunctionDefinition(std::string name, 
                       std::vector<std::pair<std::string, TypeAnnotation_ptr>> params, 
                       TypeAnnotation_ptr ret_type, 
                       Block body)
        : name(std::move(name)), 
          parameters(std::move(params)), 
          return_type(std::move(ret_type)), 
          body(std::move(body)) {};
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

// Looping

struct Loop
{
	Block body;
};

struct SimpleLoop : public Loop {
	Expression_ptr condition;
	TokenType style;

	SimpleLoop(Block body, Expression_ptr condition, TokenType style) 
	: Loop(body), condition(std::move(condition)), style(style) {};
};

struct ForInLoop : public Loop {
	Expression_ptr lhs;
	Expression_ptr iterable_expression;

	ForInLoop(Block body, Expression_ptr lhs, Expression_ptr iterable_expression)
		: Loop(body),
		lhs(std::move(lhs)),
		iterable_expression(std::move(iterable_expression)) {};
};

// Other 

struct Pass {};


struct Return {
	std::optional<Expression_ptr> expression;

	Return() 
		: expression(std::nullopt) {};

	Return(Expression_ptr expression)
		: expression(std::make_optional(std::move(expression))) {};
};


// Loop Controls 

struct LoopControl {
	TokenType type;
	std::string label;

	LoopControl(TokenType type, std::string label = "") 
		: type(type), label(label) {}
};


// Statement Variant

struct Statement {
    using StatementData = std::variant<
        std::monostate,
        ExpressionStatement,
        
	    VariableDefinition, AliasDefinition,
		EnumDefinition, FunctionDefinition, 
        
		IfBranch, ElseBranch,
		Pass, 
		
		SimpleLoop, ForInLoop, 
		Return, LoopControl
    >;

    StatementData data;

	template<typename T>
    Statement(T&& val) : data(std::forward<T>(val)) {}
};

}