#include "Salter.h"
#include "AST.h"
#include "Doctor.h"
#include "Expression.h"
#include "Statement.h"
#include "SymbolScope.h"
#include "Token.h"
#include "Workspace.h"

#include <cstddef>
#include <memory>
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

// ============================================================================
// Main Entry Point
// ============================================================================

void Salter::run(const std::vector<Module_ptr>& build_order)
{
    Doctor::get().start();

    enter_scope(ScopeType::WORKSPACE);

    for (const auto& mod : build_order)
    {
        current_module = mod;

        enter_scope(ScopeType::MODULE);
        mod->block = salt(mod->block);
        leave_scope();

        mod->save("salter");
    }

    leave_scope();

    Doctor::get().stop();
}

void Salter::enter_scope(ScopeType scope_type)
{
    auto new_scope = std::make_shared<SymbolScope>(scope_type, current_scope);

    current_scope = new_scope;
}

void Salter::leave_scope()
{
    if (current_scope != nullptr)
    {
        current_scope = current_scope->enclosing_scope;
    }
}

bool Salter::is_template(const Statement_ptr stmt) const
{
    return std::visit(
        overloaded{
            [](const FunctionDefinition& def)
            {
                return !def.generics.empty();
            },
            [](const OperatorDefinition& def)
            {
                return !def.generics.empty();
            },
            [](const ClassDefinition& def)
            {
                return !def.generics.empty();
            },
            [](const TraitDefinition& def)
            {
                return !def.generics.empty();
            },
            [](const PrimitiveDefinition& def)
            {
                return !def.generics.empty();
            },
            [](const TypeAliasDefinition& def)
            {
                return !def.generics.empty();
            },
            [](const EnumDefinition& def)
            {
                return !def.generics.empty();
            },
            [](auto&)
            {
                return false;
            }
        },
        stmt->data
    );
}

// ============================================================================
// Statement Visitors
// ============================================================================

Statement_ptr Salter::visit(Statement_ptr statement)
{
    return std::visit(
        [&](auto& node) -> Statement_ptr
        {
            if constexpr (requires { visit(node); })
            {
                return visit(node);
            }

            return statement;
        },
        statement->data
    );
}

Block Salter::salt(Block& block)
{
    Block new_block;

    for (auto& statement : block.statements)
    {
        Statement_ptr new_stmt = visit(statement);

        if (new_stmt->is<Block>())
        {
            for (auto& stmt : new_stmt->as<Block>().statements)
            {
                new_block.add(stmt);
            }

            continue;
        }

        new_block.add(new_stmt);
    }

    return new_block;
}

Statement_ptr Salter::visit(Block& block)
{
    Block new_block = salt(block);
    return make_statement(new_block);
}

Statement_ptr Salter::visit(ExpressionStatement& statement)
{
    Expression_ptr expr = visit(statement.expression);
    return make_statement(ExpressionStatement{expr});
}

Statement_ptr Salter::visit(FunctionDefinition& def)
{
    Block new_block = salt(def.block);
    std::string mangled_name = def.name + "_" + std::to_string(def.symbol->id);

    return make_statement(
        FunctionDefinition{
            mangled_name,
            def.generics,
            def.parameters,
            def.return_type,
            new_block,
            def.is_pure,
            def.symbol,
            def.overload_symbol
        }
    );
}

Statement_ptr Salter::visit(OperatorDefinition& def)
{
    Block new_block = salt(def.block);
    std::string mangled_name = def.name + "_" + std::to_string(def.symbol->id);

    return make_statement(
        FunctionDefinition{
            mangled_name,
            def.generics,
            def.operands,
            def.return_type,
            new_block,
            true,
            def.symbol,
            def.overload_symbol
        }
    );
}

Statement_ptr Salter::visit(ClassDefinition& def)
{
    enter_scope(ScopeType::CLASS);
    Block new_block = salt(def);
    leave_scope();

    return make_statement(new_block);
}

Statement_ptr Salter::visit(TraitDefinition& def)
{
    enter_scope(ScopeType::TRAIT);
    Block new_block = salt(def);
    leave_scope();

    return make_statement(new_block);
}

Statement_ptr Salter::visit(PrimitiveDefinition& def)
{
    enter_scope(ScopeType::PRIMITIVE);
    Block new_block = salt(def);
    leave_scope();

    return make_statement(new_block);
}

Block Salter::salt(TypeDefinition& def)
{
    RecordDefinition record{
        def.name + "record_",
        def.fields,
        def.symbol,
        def.overload_symbol
    };

    StatementVector results = {make_statement(record)};

    for (auto& method : def.methods)
    {
        Block new_block = salt(method.block);

        FunctionDefinition func_def{
            def.name + "_" + method.name,
            {},
            method.parameters,
            method.return_type,
            new_block,
            method.is_pure,
            method.symbol,
            method.overload_symbol
        };

        results.push_back(make_statement(func_def));
    }

    return Block{results};
}

Statement_ptr Salter::visit(Branch& stmt)
{
    enter_scope(ScopeType::BRANCH);

    Block new_block = salt(stmt.block);
    Expression_ptr new_test = nullptr;
    Statement_ptr new_alternative = nullptr;

    if (stmt.test != nullptr)
    {
        new_test = visit(stmt.test);
    }

    if (stmt.alternative != nullptr)
    {
        new_alternative = visit(stmt.alternative);
    }

    leave_scope();

    return make_statement(Branch{new_block, new_test, new_alternative});
}

Statement_ptr Salter::visit(SimpleLoop& stmt)
{
    enter_scope(ScopeType::LOOP);
    Expression_ptr test = visit(stmt.test);
    Block new_block = salt(stmt.block);
    leave_scope();

    return make_statement(SimpleLoop{test, stmt.style, new_block});
}

Statement_ptr Salter::visit(ForInLoop& stmt)
{
    enter_scope(ScopeType::LOOP);
    Expression_ptr lhs = visit(stmt.lhs);
    Block new_block = salt(stmt.block);
    leave_scope();

    return make_statement(
        ForInLoop{stmt.lhs_is_mutable, lhs, stmt.iterable, new_block}
    );
}

Statement_ptr Salter::visit(Return& stmt)
{
    if (stmt.expression.has_value())
    {
        Expression_ptr new_expr = visit(stmt.expression.value());
        return make_statement(Return{new_expr});
    }

    return make_statement(Return{});
}

// ============================================================================
// Expression Visitors
// ============================================================================

Expression_ptr Salter::visit(Expression_ptr expression)
{
    return std::visit(
        [&](auto& node) -> Expression_ptr
        {
            if constexpr (requires { visit(node); })
            {
                return visit(node);
            }

            return expression;
        },
        expression->data
    );
}

Expression_ptr Salter::visit(InterpolatedString& binding)
{
    if (binding.parts.empty())
    {
        return make_expression(StringLiteral{""});
    }

    ExpressionVector new_parts;

    for (auto& part : binding.parts)
    {
        Expression_ptr new_part = visit(part);
        new_parts.push_back(new_part);
    }

    if (new_parts.size() == 1)
    {
        return new_parts.front();
    }

    Expression_ptr result = new_parts.front();

    for (size_t i = 1; i < new_parts.size(); i++)
    {
        result = make_expression(
            Infix{result, Token(TokenType::PLUS, "+"), new_parts[i]}
        );
    }

    return result;
}

Expression_ptr Salter::visit(Call& call)
{
    if (call.callee->is<MemberAccess>())
    {
        auto& access = call.callee->as<MemberAccess>();

        Expression_ptr owner = visit(access.owner);
        ExpressionVector new_args = {owner};

        new_args
            .insert(new_args.end(), call.arguments.begin(), call.arguments.end());

        Doctor::semantics().check(
            access.member->is<Identifier>(),
            "Expected member access to be an identifier"
        );

        std::string member_name = access.member->as<Identifier>().name;
        std::string class_name = call.owner_name;

        auto new_callee = make_expression(
            Identifier{class_name + "_" + member_name}
        );

        return make_expression(Call{new_callee, call.angular_nodes, new_args});
    }

    for (auto& arg : call.arguments)
    {
        arg = visit(arg);
    }

    return make_expression(call);
}

} // namespace Wasp
