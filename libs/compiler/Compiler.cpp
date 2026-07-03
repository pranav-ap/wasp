#include "Compiler.h"
#include "AST.h"
#include "Doctor.h"
#include "Expression.h"
#include "Statement.h"
#include "Workspace.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <variant>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

Compiler::Compiler(Workspace_ptr workspace) : workspace(workspace)
{
}

// ============================================================================
// Entry Point
// ============================================================================

void Compiler::emit(const Module_ptr& module, const std::filesystem::path& output_path)
{
    Doctor::compiler().fatal_if_nullptr(module, "Module is null");

    output.clear();

    emit_runtime_includes();
    emit(module);

    std::ofstream file(output_path);

    Doctor::compiler().check(file.is_open(), "Failed to open output file: " + output_path.string());

    file << output;
    file.close();
}

void Compiler::emit_runtime_includes()
{
    emit("#include <iostream>");
    emit("#include <string>");
    emit("");
}

void Compiler::emit(const Module_ptr& module)
{
    for (const Statement_ptr stmt : module->block.statements)
    {
        emit(stmt);
    }

    emit("int main(void) { return 0; }");
}

void Compiler::emit(const std::string& line)
{
    output += line + "\n";
}

// ============================================================================
// Statements
// ============================================================================

void Compiler::emit(const Statement_ptr stmt)
{
    Doctor::compiler().fatal_if_nullptr(stmt, "Null statement in code generation");

    std::visit(
        overloaded{
            [&](const auto& s) -> std::string
            {
                if constexpr (requires { emit(s); })
                {
                    emit(s);
                }

                Doctor::compiler().fatal("Unsupported statement type in code generation");
            }
        },
        stmt->data
    );
}

void Compiler::emit(const ExpressionStatement& stmt)
{
    std::string exp = emit(stmt.expression);
    emit(exp + ";");
}

// ============================================================================
// Expressions
// ============================================================================

std::string Compiler::emit(const Expression_ptr expr)
{
    Doctor::compiler().fatal_if_nullptr(expr, "Null expression in code generation");

    return std::visit(
        overloaded{
            [&](const auto& e) -> std::string
            {
                if constexpr (requires { emit(e); })
                {
                    emit(e);
                }

                Doctor::compiler().fatal("Unsupported expression type in code generation");
            }
        },
        expr->data
    );
}

std::string Compiler::emit(const IntegerLiteral& lit)
{
    return std::to_string(lit.value);
}

std::string Compiler::emit(const FloatLiteral& lit)
{
    return std::to_string(lit.value);
}

std::string Compiler::emit(const StringLiteral& lit)
{
    return lit.value;
}

std::string Compiler::emit(const BooleanLiteral& lit)
{
    return lit.value ? "true" : "false";
}

std::string Compiler::emit(const NoneLiteral& lit)
{
    return "nullptr";
}

} // namespace Wasp
