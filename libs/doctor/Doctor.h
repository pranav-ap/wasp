#pragma once

#include <optional>
#include <source_location>
#include <string>

namespace Wasp {

enum class WaspStage { Lexer, Parser, Semantics, Captain, Compiler, VM, Native };

std::string to_string(WaspStage stage);

struct WaspError {
    WaspStage stage;
    std::string message;

    // The location in the user's Wasp script
    int wasp_line;
    int wasp_column;

    // The location in your C++ Compiler code
    std::string cpp_file;
    int cpp_line;
    std::string cpp_function;
};

class Doctor {
private:
    Doctor() = default;

    void print_error(const WaspError& err) const;

public:
    Doctor(const Doctor&) = delete;
    Doctor& operator=(const Doctor&) = delete;
    Doctor(Doctor&&) = delete;
    Doctor& operator=(Doctor&&) = delete;

    static Doctor& get() {
        static Doctor instance;
        return instance;
    }

    [[noreturn]] void fatal(
        WaspStage stage,
        const std::string& message,
        int line = 0,
        int column = 0,
        const std::source_location location = std::source_location::current()
    ) const;

    void assert(
        bool condition,
        WaspStage stage,
        const std::string& message,
        int line = 0,
        int column = 0,
        const std::source_location location = std::source_location::current()
    ) const;
    template <typename T>
    void fatal_if_nullptr(
        T ptr,
        WaspStage stage,
        const std::string& message = "",
        int line = 0,
        int column = 0,
        const std::source_location location = std::source_location::current()
    ) const {
        if (ptr == nullptr) {
            std::string final_msg = message.empty() ? "Oh shit! A nullptr" : message;
            fatal(stage, final_msg, line, column, location);
        }
    }

    template <typename T>
    void fatal_if_nullopt(
        const std::optional<T>& opt,
        WaspStage stage,
        const std::string& message = "",
        int line = 0,
        int column = 0,
        const std::source_location location = std::source_location::current()
    ) const {
        if (!opt.has_value()) {
            std::string final_msg = message.empty() ? "Oh shit! A nullopt" : message;
            fatal(stage, final_msg, line, column, location);
        }
    }

    template <typename T>
    bool is_nullptr(T ptr, WaspStage stage, int line = 0, int column = 0) const {
        if (ptr == nullptr) {
            return true;
        }
        return false;
    }

    template <typename T>
    bool is_nullopt(
        const std::optional<T>& opt, WaspStage stage, int line = 0, int column = 0
    ) const {
        if (!opt.has_value()) {
            return true;
        }
        return false;
    }
};

} // namespace Wasp