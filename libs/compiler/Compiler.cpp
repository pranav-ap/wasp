#include "Compiler.h"
#include "AST.h"
#include "Doctor.h"
#include "Expression.h"
#include "Statement.h"
#include "Symbol.h"
#include "Token.h"
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

    auto result = std::visit(
        overloaded{
            // ========== LITERALS ==========
            [&](const IntegerLiteral& lit) -> llvm::Value*
            {
                return llvm::ConstantInt::get(builder->getInt32Ty(), lit.value);
            },
            [&](const FloatLiteral& lit) -> llvm::Value*
            {
                return llvm::ConstantFP::get(builder->getDoubleTy(), lit.value);
            },
            [&](const StringLiteral& lit) -> llvm::Value*
            {
                return builder->CreateGlobalString(lit.value, lit.value);
            },
            [&](const BooleanLiteral& lit) -> llvm::Value*
            {
                return llvm::ConstantInt::get(
                    builder->getInt1Ty(),
                    lit.value ? 1 : 0
                );
            },
            [&](const NoneLiteral& lit) -> llvm::Value*
            {
                return llvm::ConstantPointerNull::get(builder->getPtrTy());
            },

            // ========== OPERATORS ==========
            [&](const Prefix& prefix) -> llvm::Value*
            {
                auto* operand = generate(prefix.operand);

                switch (prefix.op.type)
                {
                case TokenType::MINUS:
                    return builder->CreateNeg(operand, "neg");
                case TokenType::BANG:
                    return builder->CreateNot(operand, "not");
                default:
                    Doctor::compiler().fatal(
                        "Unsupported prefix operator: " + to_string(prefix.op.type)
                    );
                }
            },
            [&](const Infix& infix) -> llvm::Value*
            {
                auto* left = generate(infix.left);
                auto* right = generate(infix.right);

                switch (infix.op.type)
                {
                // Arithmetic
                case TokenType::PLUS:
                    return builder->CreateAdd(left, right, "add");
                case TokenType::MINUS:
                    return builder->CreateSub(left, right, "sub");
                case TokenType::STAR:
                    return builder->CreateMul(left, right, "mul");
                case TokenType::DIVISION:
                    return builder->CreateSDiv(left, right, "div");
                case TokenType::MOD:
                    return builder->CreateSRem(left, right, "rem");
                // Comparisons
                case TokenType::EQUAL_EQUAL:
                    return builder->CreateICmpEQ(left, right, "cmpeq");
                case TokenType::BANG_EQUAL:
                    return builder->CreateICmpNE(left, right, "cmpne");
                case TokenType::LESSER_THAN:
                    return builder->CreateICmpSLT(left, right, "cmplt");
                case TokenType::GREATER_THAN:
                    return builder->CreateICmpSGT(left, right, "cmpgt");
                case TokenType::LESSER_THAN_EQUAL:
                    return builder->CreateICmpSLE(left, right, "cmple");
                case TokenType::GREATER_THAN_EQUAL:
                    return builder->CreateICmpSGE(left, right, "cmpge");
                // Logical (short-circuit handled elsewhere)
                case TokenType::AND:
                    return builder->CreateAnd(left, right, "and");
                case TokenType::OR:
                    return builder->CreateOr(left, right, "or");
                default:
                    Doctor::compiler().fatal(
                        "Unsupported infix operator: " + to_string(infix.op.type)
                    );
                }
            },

            // ========== Variables ==========

            [&](const Binding& binding) -> llvm::Value*
            {
                return generate(binding);
            },
            [&](const Assignment& assignment) -> llvm::Value*
            {
                return generate(assignment);
            },
            [&](const Identifier& identifier) -> llvm::Value*
            {
                return generate(identifier);
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

    Doctor::compiler().fatal_if_nullptr(
        result,
        "Failed to generate code for expression"
    );

    return result;
}

llvm::Value* Compiler::generate(const Binding& binding)
{
    auto* rhs = generate(binding.rhs);

    Doctor::compiler().check(
        binding.lhs->is<Identifier>(),
        "Left-hand side of a binding must be an identifier"
    );

    const auto& identity = binding.lhs->as<Identifier>();

    Doctor::compiler().fatal_if_nullptr(
        identity.symbol,
        "Identifier in binding does not have an associated symbol"
    );

    Type_ptr var_type = identity.symbol->get_type();

    Doctor::compiler().fatal_if_nullptr(
        var_type,
        "Variable in binding does not have an associated type"
    );

    llvm::Type* var_llvm_type = generate(var_type);

    // Allocate stack space for the variable
    auto* alloca = builder->CreateAlloca(var_llvm_type, nullptr, identity.name);
    // Store the RHS value into the variable
    builder->CreateStore(rhs, alloca);

    // Store in the symbol table for later lookup
    named_values_[identity.name] = alloca;

    return rhs;
}

llvm::Value* Compiler::generate(const Assignment& assignment)
{
    const auto& ident = assignment.lhs->as<Identifier>();

    auto it = named_values_.find(ident.name);

    Doctor::compiler().check(
        it != named_values_.end(),
        "Undefined variable: " + ident.name
    );

    auto* rhs = generate(assignment.rhs);

    auto* alloca = it->second;
    builder->CreateStore(rhs, alloca);

    return rhs;
}

llvm::Value* Compiler::generate(const Identifier& ident)
{
    auto it = named_values_.find(ident.name);

    Doctor::compiler().check(
        it != named_values_.end(),
        "Undefined variable: " + ident.name
    );

    auto* alloca = it->second;
    return builder
        ->CreateLoad(alloca->getAllocatedType(), alloca, ident.name + "_load");
}

llvm::Type* Compiler::generate(Type_ptr wasp_type)
{
    Doctor::compiler().fatal_if_nullptr(
        wasp_type,
        "Cannot generate LLVM type for null Wasp type"
    );

    auto result = std::visit(
        overloaded{
            [&](const IntType_ptr&) -> llvm::Type*
            {
                return builder->getInt32Ty();
            },
            [&](const FloatType_ptr&) -> llvm::Type*
            {
                return builder->getDoubleTy();
            },
            [&](const StringType_ptr&) -> llvm::Type*
            {
                return builder->getPtrTy();
            },
            [&](const BooleanType_ptr&) -> llvm::Type*
            {
                return builder->getInt1Ty();
            },
            [&](const auto&) -> llvm::Type*
            {
                Doctor::get().fatal("Unsupported type in code generation");
            }
        },
        wasp_type->data
    );

    Doctor::compiler().fatal_if_nullptr(
        result,
        "Failed to generate LLVM type for Wasp type"
    );

    return result;
}

} // namespace Wasp
