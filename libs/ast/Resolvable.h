#pragma once

#include <memory>

namespace Wasp
{
    struct Symbol;

    struct Resolvable
    {
        mutable std::shared_ptr<Symbol> symbol = nullptr;
        virtual ~Resolvable() = default;
    };
}