#include "Doctor.h"
#include "fmt/base.h"

#include <cpptrace/basic.hpp>
#include <cpptrace/cpptrace.hpp>
#include <cstdio>
#include <cstdlib>
#include <fmt/core.h>
#include <source_location>
#include <string>

namespace Wasp
{

std::string to_string(WaspStage stage)
{
    switch (stage)
    {
    case WaspStage::Lexer:
        return "Lexer Error";
    case WaspStage::Parser:
        return "Parser Error";
    case WaspStage::Semantics:
        return "Semantic Error";
    case WaspStage::Captain:
        return "Captain Error";
    case WaspStage::Compiler:
        return "Compiler Error";
    case WaspStage::VM:
        return "Runtime Error";
    case WaspStage::Native:
        return "Native Error";
    default:
        return "Unknown Error";
    }
}

void Doctor::print_error(const WaspError& err) const
{
    fmt::print(
        stderr,
        "\033[31m{}\033[0m : {}\n",
        to_string(err.stage),
        err.message
    );

    if (err.wasp_line > 0)
    {
        if (err.wasp_column > 0)
        {
            fmt::print(
                stderr,
                "  -> at Wasp script line {}, column {}\n",
                err.wasp_line,
                err.wasp_column
            );
        }
        else
        {
            fmt::print(stderr, "  -> at Wasp script line {}\n", err.wasp_line);
        }
    }

    fmt::print(
        stderr,
        "  => [Wasp Trace] FILE     : {}:{}\n",
        err.cpp_file,
        err.cpp_line
    );
    fmt::print(stderr, "  => [Wasp Trace] FUNCTION : {}\n\n", err.cpp_function);
}

void Doctor::fatal(
    WaspStage stage,
    const std::string& message,
    int line,
    int column,
    const std::source_location location
) const
{
    WaspError err{
        stage,
        message,
        line,
        column,
        location.file_name(),
        static_cast<int>(location.line()),
        location.function_name()
    };

    print_error(err);

    // cpptrace::generate_trace().print();

    fmt::print(
        stderr,
        "\033[31mCompilation aborted due to fatal errors.\033[0m\n"
    );
    std::exit(EXIT_FAILURE);
}

void Doctor::assert(
    bool condition,
    WaspStage stage,
    const std::string& message,
    int line,
    int column,
    const std::source_location location
) const
{
    if (!condition)
    {
        fatal(stage, message, line, column, location);
    }
}

} // namespace Wasp
