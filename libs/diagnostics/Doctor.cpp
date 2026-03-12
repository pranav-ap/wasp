#include "Doctor.h"

#include <cstdlib>
#include <iostream>
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
    // ANSI color codes: \033[31;1m is bold red, \033[0m resets formatting
    std::cerr << "\033[31;1m" << to_string(err.stage) << "\033[0m : " << err.message << "\n";

    if (err.line > 0) {
        if (err.column > 0) {
            std::cerr << "  -> at line " << err.line << ", column " << err.column << "\n";
        } else {
            std::cerr << "  -> at line " << err.line << "\n";
        }
    }

    std::cerr << "\n";
}

void Doctor::fatal(WaspStage stage, const std::string& message, int line, int column) const {
    WaspError err{stage, message, line, column};
    print_error(err);

    std::cerr << "\033[31;1mCompilation aborted due to fatal errors.\033[0m\n";
    std::exit(EXIT_FAILURE);
}

void Doctor::assert_true(
    bool condition, WaspStage stage, const std::string& message, int line, int column
) const {
    if (!condition) {
        fatal(stage, message, line, column);
    }
}

} // namespace Wasp