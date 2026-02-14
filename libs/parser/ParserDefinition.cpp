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
   Statement_ptr Parser::parse_variable_definition(bool is_mutable) {
	token_pipe.advance_pointer();
	
	auto expression = parse_expression();
	token_pipe.require_in_line(TokenType::EOL);

	return MAKE_STATEMENT(VariableDefinition(expression, is_mutable));
} 

Statement_ptr Parser::parse_alias_definition() {
	token_pipe.advance_pointer();

	auto name_token = token_pipe.require_in_line(TokenType::IDENTIFIER);
	auto name = name_token.value;

	token_pipe.require_in_line(TokenType::EQUAL);

	auto ref_type = parse_type();
	token_pipe.require_in_line(TokenType::EOL);

	return MAKE_STATEMENT(AliasDefinition(name, ref_type));
}
}
