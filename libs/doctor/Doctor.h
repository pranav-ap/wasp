#pragma once

#include <optional>
#include <string>

namespace Wasp {

enum class WaspStage { Lexer, Parser, Semantics, Captain, Compiler, VM, Native };

std::string to_string(WaspStage stage);

struct WaspError {
    WaspStage stage;
    std::string message;
    int line;
    int column;
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

    [[noreturn]] void
    fatal(WaspStage stage, const std::string& message, int line = 0, int column = 0) const;

    void assert_true(
        bool condition, WaspStage stage, const std::string& message, int line = 0, int column = 0
    ) const;

    template <typename T>
    void fatal_if_nullptr(T ptr, WaspStage stage, int line = 0, int column = 0) const {
        if (ptr == nullptr) {
            fatal(stage, "Oh shit! A nullptr", line, column);
        }
    }

    template <typename T>
    void fatal_if_nullopt(
        const std::optional<T>& opt, WaspStage stage, int line = 0, int column = 0
    ) const {
        if (!opt.has_value()) {
            fatal(stage, "Oh shit! A nullopt", line, column);
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
    bool
    is_nullopt(const std::optional<T>& opt, WaspStage stage, int line = 0, int column = 0) const {
        if (!opt.has_value()) {
            return true;
        }
        return false;
    }
};

} // namespace Wasp
