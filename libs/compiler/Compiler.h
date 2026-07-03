#pragma once

#include "AST.h"
#include "Expression.h"
#include "Statement.h"
#include "Workspace.h"

#include <filesystem>
#include <string>

namespace Wasp
{

class Compiler
{
public:
    explicit Compiler(Workspace_ptr workspace);

    void emit(const Module_ptr& module, const std::filesystem::path& output_path);

private:
    Workspace_ptr workspace;
    std::string output;

private:
    void emit(const Statement_ptr stmt);
    void emit(const ExpressionStatement& stmt);

private:
    std::string emit(const Expression_ptr expr);
    std::string emit(const IntegerLiteral& lit);
    std::string emit(const FloatLiteral& lit);
    std::string emit(const StringLiteral& lit);
    std::string emit(const BooleanLiteral& lit);
    std::string emit(const NoneLiteral& lit);

private:
    // Utils

    void emit_runtime_includes();
    void emit(const Module_ptr& module);
    void emit(const std::string& line = "");
};

} // namespace Wasp
