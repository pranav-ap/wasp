#pragma once

#include "Workspace.h"

namespace Wasp
{
class Compiler
{
public:
    explicit Compiler();

    ~Compiler() = default;

    void run(Module_ptr mod);
};
} // namespace Wasp
