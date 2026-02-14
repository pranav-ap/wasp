#pragma once

#include "Expression.h"
#include "Statement.h"
#include "Precedence.h"
#include "ExpressionParselets.h"
#include "TypeAnnotation.h"
#include "Token.h"
#include "TokenPipe.h"

#include <vector>
#include <memory>
#include <map>
#include <tuple>
#include <optional>


namespace Wasp {
    struct Module {
        std::vector<Statement_ptr> statements;
    };

    class Parser {
        // Statement Parser

        Statement_ptr parse_statement(int expected_indent_level = 0);

        Statement_ptr parse_expression_statement();

        // Definition Parsers

        Statement_ptr parse_variable_definition(bool is_mutable);
        Statement_ptr parse_alias_definition();
        Statement_ptr parse_pass_statement();
        
        // Block parsers

        Statement_ptr parse_branching(TokenType token_type, int if_indent_level);
        Block parse_conditional_block(int expected_indent_level);
        Statement_ptr parse_else_block(int if_indent_level);

        // Pratt Parser

        std::map<TokenType, IPrefixParselet_ptr> prefix_parselets;
        std::map<TokenType, IInfixParselet_ptr> infix_parselets;

        void register_parselet(TokenType token_type, IPrefixParselet_ptr parselet);
        void register_parselet(TokenType token_type, IInfixParselet_ptr parselet);
        void register_prefix(TokenType token_type, Precedence precedence);
        void register_infix_left(TokenType token_type, Precedence precedence);
        void register_infix_right(TokenType token_type, Precedence precedence);

        [[nodiscard]] int get_next_operator_precedence();

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
        
        Module run(const std::vector<Token> &tokens);
    };

    using ParserPtr = std::shared_ptr<Parser>;
}