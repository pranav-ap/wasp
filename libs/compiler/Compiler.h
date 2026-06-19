#pragma once

#include "AST.h"
#include "Expression.h"
#include "Statement.h"
#include "Type.h"
#include "Workspace.h"

#include <map>
#include <string>
#include <vector>

namespace Wasp
{
class Compiler
{
public:
    explicit Compiler() {};

    ~Compiler() = default;

    void run(const std::vector<Module_ptr>& build_order);

private:
    int string_counter = 0;
    int temp_counter = 0;
    std::map<std::string, std::string> named_values;

private:
    // Statements

    std::string generate(const Statement_ptr);
    std::string generate(const Block&);

    std::string generate(const ExpressionStatement&);

    // Expressions

    std::string generate(const Expression_ptr);

    std::string generate(const Prefix&);
    std::string generate(const Infix&);

    std::string generate(const Binding&);
    std::string generate(const Assignment&);
    std::string generate(const Identifier&);

    // Types

    std::string generate(Type_ptr wasp_type);
};
} // namespace Wasp
