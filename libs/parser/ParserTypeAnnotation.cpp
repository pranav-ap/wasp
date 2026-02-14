#include "Parser.h"

#include <vector>
#include <stack>
#include <memory>
#include <utility>
#include <cmath>
#include <iostream>
#include <map>

#define RETURN_IF_NULLOPT(token) if (!token.has_value()) return nullptr;
#define EXIT_IF_NULLOPT(token) if (!token.has_value()) exit(1);
#define RETURN_IF_NULLPTR(token) if (!token) return nullptr;
#define EXIT_IF_NULLPTR(token) if (!token) exit(1);
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
using std::vector;
using std::endl;
using std::make_pair;
using std::make_shared;


namespace Wasp {

TypeAnnotation_ptr Parser::parse_type() {
    vector<TypeAnnotation_ptr> types;

    while (true) {
        TypeAnnotation_ptr type;

        if (token_pipe.consume_optional_in_line(TokenType::OPEN_SQUARE_BRACKET)) {
            type = parse_list_type();
        }
        else if (token_pipe.consume_optional_in_line(TokenType::OPEN_CURLY_BRACE)) {
            type = parse_set_or_map_type();
        }
        else if (token_pipe.consume_optional_in_line(TokenType::OPEN_PARENTHESIS)) {
            type = parse_tuple_or_fun_type();
        }
        else {
            type = consume_datatype_word();
        }

        types.push_back(type);

        if (token_pipe.consume_optional_in_line(TokenType::VERTICAL_BAR)) {
            continue;
        }

        break;
    }

    if (types.size() > 1) {
        return MAKE_RECURSIVE_TYPE(VariantTypeNode, move(types));
    }

    return move(types.front());
}

TypeAnnotationVector Parser::parse_types() {
    TypeAnnotationVector types;

    do {
        auto type = parse_type();
        types.push_back(type);
    } while (token_pipe.consume_optional_in_line(TokenType::COMMA));

    return types;
}

TypeAnnotation_ptr Parser::consume_datatype_word() {
    auto token = token_pipe.current_in_line();
    EXIT_IF_NULLOPT(token);

    switch (token.value().type) {
        case TokenType::NUMBER_LITERAL: {
            token_pipe.advance_pointer();
            auto value = std::stod(token.value().value);
            if (std::fmod(value, 1.0) == 0.0) {
                return MAKE_TYPE(IntLiteralTypeNode(static_cast<int>(value)));
            }
            return MAKE_TYPE(FloatLiteralTypeNode(value));
        }
        case TokenType::STRING_LITERAL: {
            token_pipe.advance_pointer();
            return MAKE_TYPE(StringLiteralTypeNode(token.value().value));
        }
        case TokenType::TRUE_KEYWORD: {
            token_pipe.advance_pointer();
            return MAKE_TYPE(BoolLiteralTypeNode(true));
        }
        case TokenType::FALSE_KEYWORD: {
            token_pipe.advance_pointer();
            return MAKE_TYPE(BoolLiteralTypeNode(false));
        }
        case TokenType::INT: {
            token_pipe.advance_pointer();
            return MAKE_TYPE(IntTypeNode());
        }
        case TokenType::FLOAT: {
            token_pipe.advance_pointer();
            return MAKE_TYPE(FloatTypeNode());
        }
        case TokenType::STR: {
            token_pipe.advance_pointer();
            return MAKE_TYPE(StringTypeNode());
        }
        case TokenType::BOOL: {
            token_pipe.advance_pointer();
            return MAKE_TYPE(BoolTypeNode());
        }
        case TokenType::ANY: {
            token_pipe.advance_pointer();
            return MAKE_TYPE(AnyTypeNode());
        }
        case TokenType::IDENTIFIER: {
            token_pipe.advance_pointer();
            return MAKE_TYPE(TypeIdentifierNode(token.value().value));
        }
        default: {
            std::cerr << "Unexpected token in datatype: " << to_string(token.value().type) << std::endl;
            exit(1);
        }
    }
}

TypeAnnotation_ptr Parser::parse_list_type() {
    auto type = parse_type();
    token_pipe.require_later(TokenType::CLOSE_SQUARE_BRACKET);
    return MAKE_RECURSIVE_TYPE(ListTypeNode, type);
}

TypeAnnotation_ptr Parser::parse_tuple_or_fun_type() {
    auto types = parse_types();
    token_pipe.require_later(TokenType::CLOSE_PARENTHESIS);

    if (token_pipe.consume_optional_in_line(TokenType::ARROW)) {
        auto return_type = parse_type();
        return MAKE_RECURSIVE_TYPE(FunctionTypeNode, types, return_type);
    }

    return MAKE_RECURSIVE_TYPE(TupleTypeNode, types);
}

TypeAnnotation_ptr Parser::parse_set_or_map_type() {
    auto key_type = parse_type();

    if (token_pipe.consume_optional_later(TokenType::ARROW)) {
        auto value_type = parse_type();
        token_pipe.require_later(TokenType::CLOSE_CURLY_BRACE);
        return MAKE_RECURSIVE_TYPE(MapTypeNode, key_type, value_type);
    }

    token_pipe.require_later(TokenType::CLOSE_CURLY_BRACE);
    return MAKE_RECURSIVE_TYPE(SetTypeNode, key_type);
}

}
