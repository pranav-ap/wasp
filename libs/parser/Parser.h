#pragma once

#include "AST.h"
#include "ExpressionParselets.h"
#include "Precedence.h"
#include "Statement.h"
#include "Token.h"
#include "TokenPipe.h"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace Wasp {
class Parser {
    // Statement Parser

    Statement_ptr parse_statement(int expected_indent_level = 0);

    Statement_ptr parse_expression_statement();

    // Definition Parsers

    Statement_ptr parse_variable_definition(bool is_mutable);
    Statement_ptr parse_alias_definition();
    Statement_ptr parse_pass_statement();
    Statement_ptr parse_loop_control_statement(TokenType control_type);
    Statement_ptr parse_return_statement();

    Statement_ptr parse_enum_definition(int indent_level = 0);
    std::vector<std::string> parse_enum_members(std::string stem, int indent_level);

    Statement_ptr parse_function_definition(int indent_level = 0);

    std::pair<std::map<std::string, TypeAnnotation_ptr>, std::vector<std::string>>
    parse_name_type_block(int expected_indent);
    std::pair<std::string, TypeAnnotation_ptr> parse_name_type_pair(int member_indent);

    Statement_ptr parse_class_definition(int indent_level = 0);
    Statement_ptr parse_trait_definition(int indent_level = 0);
    Statement_ptr parse_impl_definition(int indent_level = 0);

    Statement_ptr parse_annotation_definition();

    // Branching Parsers

    StatementVector parse_statements_block(int expected_indent_level);

    Statement_ptr parse_branching(TokenType token_type, int if_indent_level);
    Statement_ptr parse_else_block(int if_indent_level);

    // Looping Parsers

    Statement_ptr parse_simple_loop(TokenType loop_style, int loop_indent_level);
    Statement_ptr parse_for_in_loop(int loop_indent_level);

    // Other Parsers

    std::pair<std::optional<TokenType>, std::vector<std::string>> parse_module_path();
    ImportedSymbol parse_imported_symbol();

    Statement_ptr parse_import();
    Statement_ptr parse_from_import();

    // Pratt Parser

    std::map<TokenType, IPrefixParselet_ptr> prefix_parselets;
    std::map<TokenType, IInfixParselet_ptr> infix_parselets;

    void register_all_parselets();

    void register_parselet(TokenType token_type, IPrefixParselet_ptr parselet);
    void register_parselet(TokenType token_type, IInfixParselet_ptr parselet);
    void register_prefix(TokenType token_type, Precedence precedence);
    void register_infix_left(TokenType token_type, Precedence precedence);
    void register_infix_right(TokenType token_type, Precedence precedence);

    int get_next_operator_precedence();

    // Type Annotation Parsers

    TypeAnnotation_ptr consume_datatype_word();

    TypeAnnotation_ptr parse_list_type();
    TypeAnnotation_ptr parse_set_or_map_type();
    TypeAnnotation_ptr parse_tuple_or_fun_type();

public:
    TokenPipe token_pipe;

    Parser();

    // Expression Parser

    Expression_ptr parse_expression();
    Expression_ptr parse_expression(int precedence);
    ExpressionVector parse_expressions();

    Expression_ptr parse_ternary_condition(TokenType token_type, Expression_ptr prev_condition);

    TypeAnnotation_ptr parse_type();
    TypeAnnotationVector parse_types();

    StatementVector run(const std::vector<Token>& tokens);
};

using ParserPtr = std::shared_ptr<Parser>;
} // namespace Wasp
