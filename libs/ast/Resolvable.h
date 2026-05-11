#pragma once

#include <memory>
#include <utility>

namespace Wasp
{
    struct Symbol;

    struct Resolvable
    {
        mutable std::shared_ptr<Symbol> symbol = nullptr;
        bool must_be_captured = false;

        Resolvable() = default;

        explicit Resolvable(std::shared_ptr<Symbol> symbol)
            : symbol(std::move(symbol))
        {
        }

        virtual ~Resolvable() = default;
    };
}
