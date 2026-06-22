#include "Compiler.h"
#include "AST.h"
#include "Doctor.h"
#include "Expression.h"
#include "Statement.h"
#include "SymbolScope.h"
#include "Token.h"
#include "Type.h"
#include "Workspace.h"
#include "fmt/base.h"

#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};

template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

namespace
{

void save_to_file(std::stringstream& qbe_output)
{
    std::string output_path = "/workspaces/wasp/code/build/main.ssa";

    std::filesystem::create_directories(
        std::filesystem::path(output_path).parent_path()
    );

    std::ofstream file(output_path);

    Doctor::compiler().check(
        file.is_open(),
        "Failed to open output file: " + output_path
    );

    file << qbe_output.str();
    file.close();

    fmt::println("QBE IR written to: {}", output_path);
}

} // namespace

// ============================================================================
// Entry Point
// ============================================================================

void Compiler::run(const std::vector<Module_ptr>& build_order)
{
    Doctor::get().start();

    std::stringstream qbe_output;

    // Emit the main function
    qbe_output << "export function w $main() {\n";
    qbe_output << "@start\n";

    // Generate code for all modules inside main
    for (const auto& mod : build_order)
    {
        current_module = mod;

        std::string module_text = generate(mod->block);
        qbe_output << module_text;
    }

    // Return 0 by default
    qbe_output << "ret 0\n";
    qbe_output << "}\n";

    save_to_file(qbe_output);

    Doctor::get().stop();
}

// ============================================================================
// Statement
// ============================================================================

std::string Compiler::generate(const Statement_ptr stmt)
{
    Doctor::compiler().fatal_if_nullptr(
        stmt,
        "Cannot generate code for a null statement"
    );

    std::string text = std::visit(
        overloaded{
            [&](const ExpressionStatement& s) -> std::string
            {
                return generate(s.expression);
            },
            [&](const auto&) -> std::string
            {
                Doctor::compiler().fatal(
                    "Unsupported statement type in code generation"
                );
            }
        },
        stmt->data
    );

    return text;
}

std::string Compiler::generate(const Block& block)
{
    std::string block_text = "";

    for (const auto& s : block.statements)
    {
        std::string line = generate(s);
        block_text += line;
    }

    return block_text;
}

std::string Compiler::generate(const ExpressionStatement& stmt)
{
    auto text = generate(stmt.expression);
    return text;
}

// ============================================================================
// Expression Generation
// ============================================================================

std::string Compiler::generate(const Expression_ptr expr)
{
    Doctor::compiler().fatal_if_nullptr(
        expr,
        "Cannot generate code for null expression"
    );

    std::string text = std::visit(
        overloaded{
            // ========== LITERALS ==========
            [&](const IntegerLiteral& lit) -> std::string
            {
                return std::to_string(lit.value);
            },
            [&](const FloatLiteral& lit) -> std::string
            {
                return std::to_string(lit.value);
            },
            [&](const StringLiteral&) -> std::string
            {
                std::string name = "$str_" + std::to_string(string_counter++);
                return name;
            },
            [&](const BooleanLiteral& lit) -> std::string
            {
                return lit.value ? "1" : "0";
            },
            [&](const NoneLiteral&) -> std::string
            {
                return "0";
            },

            // ========== OPERATORS ==========
            [&](const Prefix& prefix) -> std::string
            {
                return generate(prefix);
            },
            [&](const Infix& infix) -> std::string
            {
                return generate(infix);
            },

            // ========== VARIABLES ==========
            [&](const Binding& binding) -> std::string
            {
                return generate(binding);
            },
            [&](const Assignment& assignment) -> std::string
            {
                return generate(assignment);
            },
            [&](const Identifier& ident) -> std::string
            {
                return generate(ident);
            },

            [&](const auto&) -> std::string
            {
                Doctor::get().fatal(
                    "Unsupported expression type in code generation"
                );
            }
        },
        expr->data
    );

    Doctor::compiler().fatal_if_empty_string(
        text,
        "Expression Code generation resulted in an empty string"
    );

    return text;
}

// ============================================================================
// Variable Operations
// ============================================================================

std::string Compiler::generate(const Binding& binding)
{
    std::string rhs = generate(binding.rhs);

    Doctor::compiler().check(
        binding.lhs->is<Identifier>(),
        "Binding LHS must be an identifier"
    );

    const auto& ident = binding.lhs->as<Identifier>();

    // Allocate stack space

    std::string temp = "%" + ident.name;

    std::stringstream ss;
    ss << temp << " =l alloc8 4\n"; // Allocate 4 bytes
    ss << "storew " << rhs << ", " << temp << "\n";

    // Store the pointer in the variable map
    named_values[ident.name] = temp;

    return ss.str();
}

std::string Compiler::generate(const Assignment& assignment)
{
    Doctor::compiler().check(
        assignment.lhs->is<Identifier>(),
        "Assignment LHS must be an identifier"
    );

    const Identifier& identity = assignment.lhs->as<Identifier>();

    Doctor::compiler().check(
        named_values.contains(identity.name),
        "Undefined variable: " + identity.name
    );

    std::string rhs = generate(assignment.rhs);

    std::stringstream ss;
    ss << "storew " << rhs << ", " << named_values.at(identity.name) << "\n";

    return ss.str();
}

std::string Compiler::generate(const Identifier& identifier)
{
    Doctor::compiler().check(
        named_values.contains(identifier.name),
        "Undefined variable: " + identifier.name
    );

    std::string temp = "%" + identifier.name + "_" +
                       std::to_string(identifier.symbol->id);

    return temp + " =w loadw " + named_values.at(identifier.name);
}

// ============================================================================
// Operators
// ============================================================================

std::string Compiler::generate(const Prefix& prefix)
{
    std::string operand = generate(prefix.operand);
    std::string temp = "%t" + std::to_string(temp_counter++);

    switch (prefix.op.type)
    {
    case TokenType::MINUS:
        return temp + " =w neg " + operand;
    case TokenType::BANG:
        return temp + " =w xor 1, " + operand;
    default:
        Doctor::compiler().fatal(
            "Unsupported prefix operator: " + to_string(prefix.op.type)
        );
    }
}

std::string Compiler::generate(const Infix& infix)
{
    std::string left = generate(infix.left);
    std::string right = generate(infix.right);
    std::string temp = "%t" + std::to_string(temp_counter++);

    switch (infix.op.type)
    {
    case TokenType::PLUS:
        return temp + " =w add " + left + ", " + right;
    case TokenType::MINUS:
        return temp + " =w sub " + left + ", " + right;
    case TokenType::STAR:
        return temp + " =w mul " + left + ", " + right;
    case TokenType::DIVISION:
        return temp + " =w div " + left + ", " + right;
    case TokenType::MOD:
        return temp + " =w rem " + left + ", " + right;
    case TokenType::EQUAL_EQUAL:
        return temp + " =w ceqw " + left + ", " + right;
    case TokenType::BANG_EQUAL:
        return temp + " =w cnew " + left + ", " + right;
    case TokenType::LESSER_THAN:
        return temp + " =w cwlt " + left + ", " + right;
    case TokenType::GREATER_THAN:
        return temp + " =w cwgt " + left + ", " + right;
    case TokenType::LESSER_THAN_EQUAL:
        return temp + " =w cwle " + left + ", " + right;
    case TokenType::GREATER_THAN_EQUAL:
        return temp + " =w cwge " + left + ", " + right;
    case TokenType::AND:
        return temp + " =w and " + left + ", " + right;
    case TokenType::OR:
        return temp + " =w or " + left + ", " + right;
    default:
        Doctor::compiler().fatal(
            "Unsupported infix operator: " + to_string(infix.op.type)
        );
    }
}

// ============================================================================
// Type Mapping
// ============================================================================

std::string Compiler::generate(Type_ptr wasp_type)
{
    Doctor::compiler().fatal_if_nullptr(
        wasp_type,
        "Cannot generate QBE type for null Wasp type"
    );

    return std::visit(
        overloaded{
            [&](const IntType_ptr&) -> std::string
            {
                return "w"; // word - 32-bit integer
            },
            [&](const FloatType_ptr&) -> std::string
            {
                return "s"; // single - 32-bit float
            },
            [&](const StringType_ptr&) -> std::string
            {
                return "l"; // long - 64-bit integer
            },
            [&](const BooleanType_ptr&) -> std::string
            {
                return "w"; // Boolean as word
            },
            [&](const auto&) -> std::string
            {
                Doctor::get().fatal("Unsupported type in code generation");
            }
        },
        wasp_type->data
    );
}

// ============================================================================
// Utils
// ============================================================================

void Compiler::enter_scope(ScopeType scope_type)
{
    auto new_scope = std::make_shared<SymbolScope>(scope_type, current_scope);

    current_scope = new_scope;
}

void Compiler::leave_scope()
{
    if (current_scope != nullptr)
    {
        current_scope = current_scope->enclosing_scope;
    }
}

} // namespace Wasp
