#include "Doctor.h"

#include <cpptrace/basic.hpp>
#include <cpptrace/cpptrace.hpp>
#include <cstdlib>
#include <iostream>
#include <source_location>
#include <string>

namespace Wasp {

std::string to_string(WaspStage stage) {
    switch (stage) {
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

void Doctor::print_error(const WaspError& err) const {
    std::cerr << "\033[31m" << to_string(err.stage) << "\033[0m : " << err.message << "\n";

    if (err.wasp_line > 0) {
        if (err.wasp_column > 0) {
            std::cerr << "  -> at Wasp script line " << err.wasp_line << ", column "
                      << err.wasp_column << "\n";
        } else {
            std::cerr << "  -> at Wasp script line " << err.wasp_line << "\n";
        }
    }

    std::cerr << "  => [Wasp Trace] FILE     : " << err.cpp_file << ":" << err.cpp_line << "\n";
    std::cerr << "  => [Wasp Trace] FUNCTION : " << err.cpp_function << "\n\n";
}

void Doctor::fatal(
    WaspStage stage,
    const std::string& message,
    int line,
    int column,
    const std::source_location location
) const {
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

    cpptrace::generate_trace().print();

    std::cerr << "\033[31mCompilation aborted due to fatal errors.\033[0m\n";
    std::exit(EXIT_FAILURE);
}

void Doctor::assert(
    bool condition,
    WaspStage stage,
    const std::string& message,
    int line,
    int column,
    const std::source_location location
) const {
    if (!condition) {
        // Forward the location to fatal so we know exactly which assert tripped!
        fatal(stage, message, line, column, location);
    }
}
} // namespace Wasp
