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
#include <tuple>
#include <utility>
#include <vector>

namespace Wasp
{

class Parser
{
public:
    TokenPipe token_pipe;

    Parser();

    // --- Entry Point ---
    StatementVector run(const std::vector<Token>& tokens);

    // --- Expression ---
    Expression_ptr parse_expression();
    Expression_ptr parse_expression(int precedence);
    ExpressionVector parse_expressions();
    Expression_ptr parse_ternary_condition(
        TokenType token_type,
        Expression_ptr prev_condition
    );

    // --- Type ---
    TypeAnnotation_ptr parse_type();
    TypeAnnotationVector parse_types();

private:
    // --- Statement Routing ---
    Statement_ptr parse_statement(int expected_indent_level = 0);
    StatementVector parse_statements_block(int expected_indent_level);
    Statement_ptr parse_expression_statement();

    // --- Definitions & Declarations ---
    Expression_ptr parse_variable_definition(bool is_mutable);
    Statement_ptr parse_function_definition(
        int indent_level,
        bool in_class_block = false,
        bool is_our = false,
        bool is_pure = false
    );
    Statement_ptr parse_operator_definition(TokenType fixity, int indent_level);
    Statement_ptr parse_class_definition(int indent_level = 0);
    Statement_ptr parse_trait_definition(int indent_level = 0);
    Statement_ptr parse_enum_definition(int indent_level = 0);
    Statement_ptr parse_alias_definition();
    Statement_ptr parse_template_definition(int indent_level = 0);

    // --- OOP & Member Helpers ---
    std::tuple<std::string, TypeAnnotationVector, StatementVector>
    parse_membered_definition_base(int indent_level);
    EnumDefinition parse_enum_body(std::string name, int indent_level);
    StatementVector parse_name_type_block(int expected_indent);
    std::pair<std::string, TypeAnnotation_ptr> parse_name_type_pair(
        int member_indent
    );

    // --- Control Flow & Branching ---
    Statement_ptr parse_branching(TokenType token_type, int if_indent_level);
    Statement_ptr parse_else_block(int if_indent_level);
    Statement_ptr parse_simple_loop(
        TokenType loop_style,
        int loop_indent_level
    );
    Statement_ptr parse_for_in_loop(int loop_indent_level);
    Statement_ptr parse_loop_control_statement(TokenType control_type);
    Statement_ptr parse_return_statement();
    Statement_ptr parse_placeholder(TokenType placeholder_type);

    // --- Modules & Imports ---
    Statement_ptr parse_import();
    std::tuple<std::optional<TokenType>, int, StringVector> parse_module_path();
    ImportAsPair parse_imported_symbol();

    // --- Internal Type Parsing Helpers ---
    TypeAnnotation_ptr consume_datatype_word();
    TypeAnnotation_ptr parse_list_type();
    TypeAnnotation_ptr parse_set_or_map_type();
    TypeAnnotation_ptr parse_tuple_or_fun_type();

    TypeAnnotation_ptr parse_base_type();
    TypeAnnotation_ptr parse_intersection_type();
    TypeAnnotation_ptr parse_variant_type();

    // --- Pratt Parser Registry ---
    std::map<TokenType, IPrefixParselet_ptr> prefix_parselets;
    std::map<TokenType, IInfixParselet_ptr> infix_parselets;

    void register_all_parselets();
    void register_parselet(TokenType token_type, IPrefixParselet_ptr parselet);
    void register_parselet(TokenType token_type, IInfixParselet_ptr parselet);
    void register_prefix(TokenType token_type, Precedence precedence);
    void register_infix_left(TokenType token_type, Precedence precedence);
    void register_infix_right(TokenType token_type, Precedence precedence);

    int get_next_operator_precedence();
};

using ParserPtr = std::shared_ptr<Parser>;

} // namespace Wasp
