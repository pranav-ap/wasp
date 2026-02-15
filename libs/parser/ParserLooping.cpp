#include "Parser.h"

#include <vector>
#include <stack>
#include <memory>
#include <utility>
#include <iostream>
#include <map>

#define RETURN_IF_NULLOPT(token) if (!token.has_value()) return nullptr;
#define EXIT_IF_NULLOPT(token) if (!token.has_value()) exit(1);
#define RETURN_IF_NULLPTR(token) if (!token) return nullptr;
#define EXIT_IF_NULLPTR(token) if (!token) exit(1);
#define CASE(token_type, call) case token_type: { return call; }
#define MAKE_STATEMENT(x) std::make_shared<Statement>(Statement(x))
#define MAKE_EXPRESSION(x) std::make_shared<Expression>(Expression(x))

template<class... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};

template<class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

using std::move;
using std::cout;
using std::optional;
using std::endl;
using std::make_pair;
using std::make_shared;

namespace Wasp {
Statement_ptr Parser::parse_simple_loop(TokenType loop_style, int loop_indent_level) {
	token_pipe.advance_pointer();
	
	auto condition = parse_expression();
	EXIT_IF_NULLPTR(condition);

	token_pipe.require_in_line(TokenType::DO);
	
	if (token_pipe.consume_optional_in_line(TokenType::EOL)) {
		auto body = parse_statements_block(loop_indent_level + 1);
		return MAKE_STATEMENT(SimpleLoop(body, condition, loop_style));
	}

	auto statement = parse_expression_statement();
	auto body = { statement };
	return MAKE_STATEMENT(SimpleLoop(body, condition, loop_style));
}


Statement_ptr Parser::parse_for_in_loop(int loop_indent_level) {
    // Consume the 'for' keyword
    token_pipe.advance_pointer();

    // Parse the 'x in y' part as an expression
    auto expression = parse_expression();
    EXIT_IF_NULLPTR(expression);

    // Verify we actually got an Infix expression
    if (!expression->is<Infix>()) {
        std::cerr << "Error: Expected 'EXPR in ITERABLE' after for" << std::endl;
        exit(1);
    }

    const auto& infix = expression->as<Infix>();

    // Verify the operator is 'in'
    if (infix.op.type != TokenType::IN_KEYWORD) {
        std::cerr << "Error: Expected 'in' keyword in for loop, but got " << to_string(infix.op.type) << std::endl;
        exit(1);
    }

    Expression_ptr lhs = infix.left;
    Expression_ptr iterable_expression = infix.right;

    token_pipe.require_in_line(TokenType::DO);

    // Handle block body 
    if (token_pipe.consume_optional_in_line(TokenType::EOL)) {
        auto body = parse_statements_block(loop_indent_level + 1);
        return MAKE_STATEMENT(ForInLoop(body, lhs, iterable_expression));
    }

    // Handle single-line body
    auto statement = parse_expression_statement();
    EXIT_IF_NULLPTR(statement);
    
    Block body = { statement };
    return MAKE_STATEMENT(ForInLoop(body, lhs, iterable_expression));
}

Statement_ptr Parser::parse_loop_control_statement(TokenType control_type) {
    token_pipe.advance_pointer();

    auto token = token_pipe.consume_optional_in_line(TokenType::IDENTIFIER);

    if (token.has_value() && token.value().type == TokenType::IDENTIFIER) {
        std::string label = token.value().value;
        token_pipe.require_in_line(TokenType::EOL);
        return MAKE_STATEMENT((LoopControl { control_type, label }));
    }

    token_pipe.require_in_line(TokenType::EOL);
    return MAKE_STATEMENT((LoopControl { control_type }));
}
}
