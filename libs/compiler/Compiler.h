#pragma once

#include "AST.h"
#include "Expression.h"
#include "Statement.h"
#include "Type.h"
#include "Workspace.h"

#include "llvm/IR/Type.h"
#include "llvm/IR/Value.h"
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>

#include <memory>
#include <vector>

namespace Wasp
{
class Compiler
{
public:
    explicit Compiler();

    ~Compiler() = default;

    void run(const std::vector<Module_ptr>& build_order);

private:
    std::unique_ptr<llvm::LLVMContext> context;
    std::unique_ptr<llvm::IRBuilder<>> builder;
    std::unique_ptr<llvm::Module> llvm_module;

    // Statements

    void generate(const Statement_ptr& stmt);
    void generate(const Block& stmt);

    void generate(const ExpressionStatement& stmt);

    // Expressions

    llvm::Value* generate(const Expression_ptr& expr);

    llvm::Value* generate(const IntegerLiteral& lit);
    llvm::Value* generate(const FloatLiteral& lit);
    llvm::Value* generate(const StringLiteral& lit);
    llvm::Value* generate(const BooleanLiteral& lit);

    // Types

    llvm::Type* generate(Type_ptr wasp_type);
};
} // namespace Wasp
