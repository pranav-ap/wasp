#include "Compiler.h"
#include "Doctor.h"
#include "Workspace.h"
#include "fmt/base.h"

namespace Wasp
{

Compiler::Compiler()
{
}

void Compiler::run(Module_ptr mod)
{
    Doctor::get().start();

    double time_taken = Doctor::get().stop();
    fmt::print(stdout, "Code generation completed in {:.2f} seconds.\n", time_taken);
}

} // namespace Wasp
