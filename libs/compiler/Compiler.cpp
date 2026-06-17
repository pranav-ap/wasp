#include "Compiler.h"
#include "AST.h"
#include "Doctor.h"
#include "Expression.h"
#include "Statement.h"
#include "Type.h"
#include "Workspace.h"

#include "llvm/ADT/StringRef.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Value.h"
#include "llvm/Support/raw_ostream.h"
#include <llvm/Config/llvm-config.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>

#include <llvm/Config/llvm-config.h>
#include <memory>
#include <string>
#include <system_error>
#include <variant>
#include <vector>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};

template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

Compiler::Compiler()
{
    context = std::make_unique<llvm::LLVMContext>();
    builder = std::unique_ptr<llvm::IRBuilder<>>(new llvm::IRBuilder<>(*context));
    llvm_module = std::make_unique<llvm::Module>("main", *context);

    llvm::InitializeAllTargetInfos();
    llvm::InitializeAllTargets();
    llvm::InitializeAllTargetMCs();
    llvm::InitializeAllAsmParsers();
    llvm::InitializeAllAsmPrinters();
}

void Compiler::run(const std::vector<Module_ptr>& build_order)
{
    Doctor::get().start();

    // Create implicit main function
    auto main_type = llvm::FunctionType::get(
        builder->getInt32Ty(), // return type: int
        {},                    // no parameters
        false
    );

    auto main_func = llvm::Function::Create(
        main_type,
        llvm::Function::ExternalLinkage,
        "main",
        llvm_module.get()
    );

    auto entry = llvm::BasicBlock::Create(*context, "entry", main_func);
    builder->SetInsertPoint(entry);

    for (const auto& mod : build_order)
    {
        generate(mod->block);
    }

    builder->CreateRet(llvm::ConstantInt::get(builder->getInt32Ty(), 0));

    std::string error;
    llvm::raw_string_ostream error_stream(error);

    if (llvm::verifyModule(*llvm_module, &error_stream))
    {
        Doctor::compiler().fatal("Module verification failed: " + error);
    }

    std::error_code EC;
    llvm::raw_fd_ostream file_out(
        "/workspaces/wasp/code/build/main.ll",
        EC,
        llvm::sys::fs::OF_Text
    );

    if (!EC)
    {
        llvm_module->print(file_out, nullptr);
        file_out.close();
    }

    // llvm_module->print(llvm::outs(), nullptr);

    Doctor::get().stop();
}

void Compiler::generate(const Statement_ptr& stmt)
{
    Doctor::compiler().fatal_if_nullptr(
        stmt,
        "Cannot generate code for a null statement"
    );

    std::visit(
        overloaded{
            [&](const ExpressionStatement& s)
            {
                generate(s);
            },
            [&](const auto&)
            {
                Doctor::compiler().fatal(
                    "Unsupported statement type in code generation"
                );
            }
        },
        stmt->data
    );
}

void Compiler::generate(const Block& block)
{
    for (const auto& s : block.statements)
    {
        generate(s);
    }
}

void Compiler::generate(const ExpressionStatement& stmt)
{
    auto* value = generate(stmt.expression);

    Doctor::compiler().fatal_if_nullptr(
        value,
        "Failed to generate expression statement"
    );
}

llvm::Value* Compiler::generate(const Expression_ptr& expr)
{
    Doctor::compiler().fatal_if_nullptr(
        expr,
        "Cannot generate code for null expression"
    );

    return std::visit(
        overloaded{
            [&](const IntegerLiteral& lit) -> llvm::Value*
            {
                return generate(lit);
            },
            [&](const FloatLiteral& lit) -> llvm::Value*
            {
                return generate(lit);
            },
            [&](const StringLiteral& lit) -> llvm::Value*
            {
                return generate(lit);
            },
            [&](const BooleanLiteral& lit) -> llvm::Value*
            {
                return generate(lit);
            },
            [&](const NoneLiteral& lit) -> llvm::Value*
            {
                return llvm::ConstantPointerNull::get(builder->getPtrTy());
            },
            [&](const auto&) -> llvm::Value*
            {
                Doctor::get().fatal(
                    "Unsupported expression type in code generation"
                );
            }
        },
        expr->data
    );
}

llvm::Value* Compiler::generate(const IntegerLiteral& lit)
{
    return llvm::ConstantInt::get(builder->getInt32Ty(), lit.value);
}

llvm::Value* Compiler::generate(const FloatLiteral& lit)
{
    return llvm::ConstantFP::get(builder->getDoubleTy(), lit.value);
}

llvm::Value* Compiler::generate(const StringLiteral& lit)
{
    return builder->CreateGlobalString(lit.value, lit.value);
}

llvm::Value* Compiler::generate(const BooleanLiteral& lit)
{
    return llvm::ConstantInt::get(builder->getInt1Ty(), lit.value ? 1 : 0);
}

llvm::Type* Compiler::generate(Type_ptr wasp_type)
{
    Doctor::compiler().fatal_if_nullptr(
        wasp_type,
        "Cannot generate LLVM type for null Wasp type"
    );

    if (wasp_type->is<IntType_ptr>())
    {
        return builder->getInt32Ty();
    }
    if (wasp_type->is<FloatType_ptr>())
    {
        return builder->getDoubleTy();
    }
    if (wasp_type->is<StringType_ptr>())
    {
        return builder->getPtrTy();
    }
    if (wasp_type->is<BooleanType_ptr>())
    {
        return builder->getInt1Ty();
    }

    return nullptr;
}

} // namespace Wasp
