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
    const std::source_location location
) const
{
    WaspError err{
        stage,
        message,
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
    const std::source_location location
) const
{
    if (!condition)
    {
        fatal(stage, message, location);
    }
}

} // namespace Wasp
