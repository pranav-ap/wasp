#pragma once

#include "AST.h"
#include "Expression.h"
#include "Statement.h"
#include "Type.h"
#include "Workspace.h"

#include <string>
#include <vector>

namespace Wasp
{

class Compiler
{
public:
    explicit Compiler(Workspace_ptr workspace);

    void run(std::vector<Module_ptr>& build_order);

private:
    Workspace_ptr workspace;
    std::string output;

private:
    void emit(const Module_ptr& module);

    void emit(const Statement_ptr stmt);
    void emit(const ExpressionStatement& stmt);

private:
    std::string emit(const Expression_ptr expr);
    std::string emit(const IntegerLiteral& lit);
    std::string emit(const FloatLiteral& lit);
    std::string emit(const StringLiteral& lit);
    std::string emit(const BooleanLiteral& lit);
    std::string emit(const NoneLiteral& lit);

    std::string emit(const Binding& binding);
    std::string emit(const Assignment& assignment);

private:
    // Utils

    std::string emit(const Type_ptr& type);

    void emit_common_includes();
    void emit(const std::string& line = "");
};

} // namespace Wasp
