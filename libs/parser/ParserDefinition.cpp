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
#define MAKE_TYPE(x) std::make_shared<TypeAnnotation>(x)
#define MAKE_RECURSIVE_TYPE(T, ...) std::make_shared<TypeAnnotation>(std::make_shared<T>(__VA_ARGS__))



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


// Enum

Statement_ptr Parser::parse_enum_definition(int indent_level) {
	token_pipe.advance_pointer();

	Token identifier = token_pipe.require_in_line(TokenType::IDENTIFIER);
	token_pipe.require_in_line(TokenType::EOL);

	std::vector<std::string> members = parse_enum_members(identifier.value, indent_level + 1);
	return MAKE_STATEMENT(EnumDefinition(identifier.value, members));
}

std::vector<std::string> Parser::parse_enum_members(std::string stem, int indent_level) {
    std::vector<std::string> members;

    while (true) {
		token_pipe.ignore_empty_lines();
		
        // Check indentation at the start of each line
        if (token_pipe.lookahead_indents() != indent_level) {
			// End of this enum block
            break; 
        }

        token_pipe.expect_n_indents(indent_level);

        // Nested Enum
        if (token_pipe.consume_optional(TokenType::ENUM)) {
            auto nested_name = token_pipe.require_in_line(TokenType::IDENTIFIER).value;
            token_pipe.require_in_line(TokenType::EOL);
            
            // Recurse with extended stem and deeper indentation
            auto nested_members = parse_enum_members(stem + "." + nested_name, indent_level + 1);
            members.insert(members.end(), nested_members.begin(), nested_members.end());
			continue;
        } 

        // Leaf Member
        if (auto token = token_pipe.consume_optional(TokenType::IDENTIFIER)) {
            members.push_back(stem + "." + token->value);
            token_pipe.require_in_line(TokenType::EOL);
			continue;
        } 

		// No more members found
		break; 
    }

    return members;
}

// Function

Statement_ptr Parser::parse_function_definition(int indent_level) {
    token_pipe.advance_pointer();

    auto name_token = token_pipe.require_in_line(TokenType::IDENTIFIER);
    auto name = name_token.value;

    token_pipe.require_in_line(TokenType::OPEN_PARENTHESIS);

    std::vector<std::pair<std::string, TypeAnnotation_ptr>> parameters;
    if (!token_pipe.consume_optional(TokenType::CLOSE_PARENTHESIS)) {
        while (true) {
            auto param_name_token = token_pipe.require_in_line(TokenType::IDENTIFIER);
            auto param_name = param_name_token.value;

            token_pipe.require_in_line(TokenType::COLON);

            auto param_type = parse_type();

            parameters.push_back(make_pair(param_name, param_type));

            if (token_pipe.consume_optional(TokenType::CLOSE_PARENTHESIS)) {
                break;
            }

            token_pipe.require_in_line(TokenType::COMMA);
        }
    }

    TypeAnnotation_ptr return_type = nullptr;
    if (token_pipe.consume_optional_in_line(TokenType::ARROW)) {
        return_type = parse_type();
    }

    token_pipe.require_in_line(TokenType::EOL);

    Block body = parse_statements_block(indent_level + 1);
    return MAKE_STATEMENT(FunctionDefinition(name, parameters, return_type, body));
}

Statement_ptr Parser::parse_annotation_definition() {
    token_pipe.advance_pointer(); 

    auto name_token = token_pipe.require_in_line(TokenType::IDENTIFIER);
    auto name = name_token.value;

    if (token_pipe.consume_optional_in_line(TokenType::OPEN_PARENTHESIS)) {
        std::vector<Expression_ptr> args = parse_expressions(); 
        token_pipe.require_in_line(TokenType::CLOSE_PARENTHESIS);
        return MAKE_STATEMENT(AnnotationDefinition(name, args));
    }

    return MAKE_STATEMENT(AnnotationDefinition(name, {}));
}

// Class

std::map<std::string, TypeAnnotation_ptr> Parser::parse_name_type_block(int expected_indent) {
    std::map<std::string, TypeAnnotation_ptr> members;

    while (true) {
        token_pipe.ignore_empty_lines();
        
        if (token_pipe.lookahead_indents() != expected_indent) {
            break;
        }

        token_pipe.expect_n_indents(expected_indent);
        
        auto [member_name, member_type] = parse_name_type_pair(expected_indent);
        members[member_name] = member_type;
    }

    return members;
}

std::pair<std::string, TypeAnnotation_ptr> Parser::parse_name_type_pair(int member_indent) {
    auto name_token = token_pipe.require_in_line(TokenType::IDENTIFIER);
    std::string name = name_token.value;

    if (token_pipe.consume_optional_in_line(TokenType::RECORD)) {
        token_pipe.require_in_line(TokenType::EOL);
        const int record_indent = member_indent + 1;
        std::map<std::string, TypeAnnotation_ptr> record_members;

        while (true) {
            token_pipe.ignore_empty_lines();
            
            if (token_pipe.lookahead_indents() != record_indent) {
                break;
            }

            token_pipe.expect_n_indents(record_indent);
            auto [member_name, member_type] = parse_name_type_pair(record_indent);
            record_members[member_name] = member_type;
        }
        
        return make_pair(name, MAKE_RECURSIVE_TYPE(RecordTypeNode, record_members));
    }

    token_pipe.require_in_line(TokenType::COLON);
    auto type = parse_type();

    return make_pair(name, type);
}

Statement_ptr Parser::parse_class_definition(int indent_level) {
    token_pipe.advance_pointer(); // Consume 'class' keyword

    auto name_token = token_pipe.require_in_line(TokenType::IDENTIFIER);
    auto class_name = name_token.value;

    token_pipe.require_in_line(TokenType::EOL);

    // Parse all members using the abstracted block loop
    auto members = parse_name_type_block(indent_level + 1);

    return MAKE_STATEMENT(ClassDefinition(class_name, members));
}

}
