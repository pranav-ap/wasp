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
    Statement_ptr Parser::parse_statement(int expected_indent_level) {
        token_pipe.ignore_empty_lines();
        token_pipe.expect_n_indents(expected_indent_level);

        const auto token = token_pipe.current();
        RETURN_IF_NULLOPT(token);

        if (token.value().type == TokenType::END_OF_FILE) {
            token_pipe.advance_pointer();
            return nullptr;
        }

        switch (token->type) {
			CASE(TokenType::LET, parse_variable_definition(true));
			CASE(TokenType::CONST_KEYWORD, parse_variable_definition(false));
            CASE(TokenType::ALIAS, parse_alias_definition());
            
		    CASE(TokenType::IF, parse_branching(
                token.value().type, 
                expected_indent_level)
            );
            CASE(TokenType::PASS, parse_pass_statement());

            default:
                return parse_expression_statement();
        }
    }

    Statement_ptr Parser::parse_expression_statement() {
        const auto expression = parse_expression();       
        token_pipe.require_in_line(TokenType::EOL);

        return MAKE_STATEMENT((ExpressionStatement {
            expression
        }));
    }

    Statement_ptr Parser::parse_pass_statement() {
        token_pipe.advance_pointer();
        token_pipe.require_in_line(TokenType::EOL);

        return MAKE_STATEMENT((Pass {}));
    }
}
