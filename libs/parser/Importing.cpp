#include "AST.h"
#include "Parser.h"
#include "Statement.h"
#include "Token.h"

#include <cstdlib>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace Wasp
{

ImportAsPair Parser::parse_imported_symbol()
{
    auto name = token_pipe.require_in_line(TokenType::IDENTIFIER).lexeme;

    if (token_pipe.consume_optional_in_line(TokenType::AS))
    {
        return ImportAsPair(
            std::move(name),
            token_pipe.require_in_line(TokenType::IDENTIFIER).lexeme
        );
    }

    return ImportAsPair(std::move(name));
}

std::tuple<std::optional<TokenType>, int, StringVector> Parser::
    parse_module_path()
{
    std::optional<TokenType> access_modifier = std::nullopt;
    int jumps = 1; // Defaults to 1 for standard my/our/pkg/top/up
    StringVector path;

    auto current = token_pipe.current_in_line();

    if (current &&
        (current->type == TokenType::MY || current->type == TokenType::OUR ||
         current->type == TokenType::UP || current->type == TokenType::PKG ||
         current->type == TokenType::TOP))
    {
        access_modifier = current->type;
        token_pipe.advance_pointer();

        // Check for the integer argument, e.g., up(2)
        if (token_pipe.consume_optional_in_line(TokenType::OPEN_PARENTHESIS))
        {
            auto num_token = token_pipe.require_in_line(
                TokenType::NUMBER_LITERAL
            );
            jumps = std::stoi(num_token.lexeme);
            token_pipe.require_in_line(TokenType::CLOSE_PARENTHESIS);
        }

        token_pipe.require_in_line(TokenType::DOT);
    }

    do
    {
        path.push_back(
            token_pipe.require_in_line(TokenType::IDENTIFIER).lexeme
        );
    }
    while (token_pipe.consume_optional_in_line(TokenType::DOT));

    return {access_modifier, jumps, path};
}

Statement_ptr Parser::parse_import()
{
    // Consume 'import'
    token_pipe.advance_pointer();

    auto [access_modifier, access_arg, path] = parse_module_path();

    std::optional<std::string> module_alias = std::nullopt;
    bool expose_all = false;
    std::vector<ImportAsPair> exposed_names;
    StringVector excluded_names;

    // Check for module alias (import X as x)
    if (token_pipe.consume_optional_in_line(TokenType::AS))
    {
        module_alias = token_pipe.require_in_line(TokenType::IDENTIFIER).lexeme;
    }

    // Check for exposed symbols (expose a, b, c)
    if (token_pipe.consume_optional_in_line(TokenType::EXPOSE))
    {
        // Expose everything (*)
        if (token_pipe.consume_optional_in_line(TokenType::STAR))
        {
            expose_all = true;

            // Expose everything EXCEPT specific symbols
            if (token_pipe.consume_optional_in_line(TokenType::EXCEPT))
            {
                do
                {
                    excluded_names.push_back(
                        token_pipe.require_in_line(TokenType::IDENTIFIER).lexeme
                    );
                }
                while (token_pipe.consume_optional_in_line(TokenType::COMMA));
            }
        }
        // Expose specific symbols explicitly
        else
        {
            do
            {
                exposed_names.push_back(parse_imported_symbol());
            }
            while (token_pipe.consume_optional_in_line(TokenType::COMMA));
        }
    }

    token_pipe.require_in_line(TokenType::EOL);

    return make_statement(Import(
        access_modifier,
        access_arg,

        std::move(path),

        std::move(module_alias),
        expose_all,
        std::move(exposed_names),
        std::move(excluded_names)
    ));
}

} // namespace Wasp
