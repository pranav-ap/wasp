#pragma once

#include "fmt/base.h"
#include <chrono>
#include <fmt/core.h>
#include <optional>
#include <source_location>
#include <string>

namespace Wasp {

enum class WaspStage
{
    Lexer,
    Parser,
    Semantics,
    Captain,
    Compiler
};

std::string to_string(WaspStage stage);

struct WaspError {
    WaspStage stage;
    std::string message;

    // The location in your C++ Compiler code
    std::string cpp_file;
    int cpp_line;
    std::string cpp_function;
};

class Doctor
{
private:
    std::chrono::steady_clock::time_point timer_start;
    static inline WaspStage current_stage = WaspStage::Lexer;

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

    static Doctor& lexer()
    {
        current_stage = WaspStage::Lexer;
        return get();
    }

    static Doctor& parser()
    {
        current_stage = WaspStage::Parser;
        return get();
    }

    static Doctor& semantics()
    {
        current_stage = WaspStage::Semantics;
        return get();
    }

    static Doctor& captain()
    {
        current_stage = WaspStage::Captain;
        return get();
    }

    static Doctor& compiler()
    {
        current_stage = WaspStage::Compiler;
        return get();
    }

    [[noreturn]] void fatal(
        const std::string& message = "",
        const std::source_location location =
            std::source_location::current()
    ) const;

    void check(
        bool condition,
        const std::string& message = "",
        const std::source_location location = std::source_location::current()
    ) const;

    template <typename T>
    void fatal_if_nullptr(
        T ptr,
        const std::string& message = "",
        const std::source_location location =
            std::source_location::current()
    ) const
    {
        if (ptr == nullptr) {
            std::string final_msg = message.empty() ? "Oh shit! A nullptr" : message;
            fatal(final_msg, location);
        }
    }

    template <typename T>
    void fatal_if_nullopt(
        const std::optional<T>& opt,
        const std::string& message = "",
        const std::source_location location =
            std::source_location::current()
    ) const
    {
        if (!opt.has_value()) {
            std::string final_msg = message.empty() ? "Oh shit! A nullopt" : message;
            fatal(final_msg, location);
        }
    }

    void fatal_if_empty_string(
        const std::string& text,
        const std::string& message = "",
        const std::source_location location = std::source_location::current()
    ) const
    {
        if (text.empty())
        {
            std::string final_msg = message.empty() ? "Didn't expect an empty string"
                                                    : message;

            fatal(final_msg, location);
        }
    }

    template <typename T> bool is_nullptr(T ptr, WaspStage) const
    {
        if (ptr == nullptr) {
            return true;
        }
        return false;
    }

    template <typename T>
    bool is_nullopt(const std::optional<T>& opt, WaspStage) const
    {
        if (!opt.has_value()) {
            return true;
        }
        return false;
    }

    void start(std::string text = "")
    {
        if (text.empty())
        {
            fmt::print(stdout, "Starting {} stage...\n", to_string(current_stage));
        }
        else
        {
            fmt::print(
                stdout,
                "Starting {} stage: {}...\n",
                to_string(current_stage),
                text
            );
        }

        timer_start = std::chrono::steady_clock::now();
    }

    void stop()
    {
        auto end = std::chrono::steady_clock::now();

        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
            end - timer_start
        );

        double elapsed_ms = duration.count() / 1000.0;
        double elapsed_s = duration.count() / 1000000.0;

        fmt::print(
            stdout,
            "Completed in {:.2f} ms ({:.3f} s).\n",
            elapsed_ms,
            elapsed_s
        );
    }
};

} // namespace Wasp
