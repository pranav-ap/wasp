#include "NameResolution.h"
#include "AST.h"
#include "Doctor.h"
#include "Expression.h"
#include "Statement.h"
#include "SymbolScope.h"
#include "TypeAnnotation.h"
#include "Workspace.h"

#include <memory>
#include <variant>
#include <vector>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

void NameResolution::run(Module_ptr mod)
{
    current_module = mod;

    enter_scope(ScopeType::MODULE);
    visit(current_module->block);
    leave_scope();
}

void NameResolution::enter_scope(ScopeType scope_type)
{
    auto new_scope = std::make_shared<SymbolScope>(
        scope_type,
        current_scope
    );

    current_scope = new_scope;
}

void NameResolution::leave_scope()
{
    if (current_scope != nullptr)
    {
        current_scope = current_scope->enclosing_scope;
    }
}

void NameResolution::visit(Block& block)
{
    for (auto& statement : block.statements)
    {
        visit(statement);
    }
}

void NameResolution::visit(Statement_ptr statement)
{
    std::visit(
        [&](auto& node)
        {
            if constexpr (requires { visit(node); })
            {
                visit(node);
            }
        },
        statement->data
    );
}

void NameResolution::visit(Import&)
{
}

void NameResolution::visit(FunctionDefinition& def)
{
    current_scope->define(def.symbol);
}

void NameResolution::visit(OperatorDefinition& def)
{
    current_scope->define(def.symbol);
}

void NameResolution::visit(ClassDefinition& def)
{
    current_scope->define(def.symbol);
}

void NameResolution::visit(TraitDefinition& def)
{
    current_scope->define(def.symbol);
}

void NameResolution::visit(PrimitiveDefinition& def)
{
    current_scope->define(def.symbol);
}

void NameResolution::visit(EnumDefinition& def)
{
    current_scope->define(def.symbol);
}

void NameResolution::visit(TypeAliasDefinition& def)
{
    current_scope->define(def.symbol);
}

void NameResolution::visit(Branch& stmt)
{
    enter_scope(ScopeType::BRANCH);
    visit(stmt.test);
    visit(stmt.block);
    leave_scope();
}

void NameResolution::visit(SimpleLoop& stmt)
{
    enter_scope(ScopeType::LOOP);
    visit(stmt.test);
    visit(stmt.block);
    leave_scope();
}

void NameResolution::visit(ForInLoop& stmt)
{
    enter_scope(ScopeType::LOOP);
    visit(stmt.lhs);
    visit(stmt.block);
    leave_scope();
}

void NameResolution::visit(Return& statement)
{
    if (statement.expression.has_value())
    {
        visit(statement.expression.value());
    }
}

void NameResolution::visit(ExpressionStatement& statement)
{
    visit(statement.expression);
}

// ============================================================================
// Expressions
// ============================================================================

void NameResolution::visit(std::vector<Expression_ptr>& expressions)
{
    for (auto& expr : expressions)
    {
        visit(expr);
    }
}

void NameResolution::visit(Expression_ptr expression)
{
    std::visit(
        [&](auto& node)
        {
            if constexpr (requires { visit(node); })
            {
                visit(node);
            }
        },
        expression->data
    );
}

void NameResolution::visit(Binding& binding)
{
    Doctor::get().assert(
        binding.lhs->is<Identifier>(),
        WaspStage::Parser,
        "Left-hand side of a binding must be an identifier"
    );

    auto& id = binding.lhs->as<Identifier>();

    current_scope->define(id.symbol);
}

void NameResolution::visit(Assignment& assignment)
{
    Doctor::get().assert(
        assignment.lhs->is<Identifier>(),
        WaspStage::Parser,
        "Left-hand side of an assignment must be an identifier"
    );

    auto& id = assignment.lhs->as<Identifier>();

    auto symbol = current_scope->lookup(id.name);

    Doctor::get().fatal_if_nullptr(
        symbol,
        WaspStage::Semantics,
        "Undefined variable: " + id.name
    );

    id.symbol = symbol;
}

void NameResolution::visit(TernaryExpression& expr)
{
    enter_scope(ScopeType::BRANCH);
    visit(expr.test);
    visit(expr.then_expr);
    visit(expr.else_expr);
    leave_scope();
}

void NameResolution::visit(Identifier& expr)
{
    auto symbol = current_scope->lookup(expr.name);

    Doctor::get().fatal_if_nullptr(
        symbol,
        WaspStage::Semantics,
        "Undefined variable: " + expr.name
    );

    expr.symbol = symbol;
}

void NameResolution::visit(MemberAccess& expr)
{
    visit(expr.object);
}

void NameResolution::visit(ListLiteral& expr)
{
    visit(expr.expressions);
}

void NameResolution::visit(TupleLiteral& expr)
{
    visit(expr.expressions);
}

void NameResolution::visit(MapLiteral& expr)
{
    for (auto& [key, value] : expr.pairs)
    {
        visit(key);
        visit(value);
    }
}

void NameResolution::visit(SetLiteral& expr)
{
    visit(expr.expressions);
}

void NameResolution::visit(Call& call)
{
    visit(call.callee);
    visit(call.arguments);
}

void NameResolution::visit(Constructor& constructor)
{
    visit(constructor.constructible);
    visit(constructor.arguments);
}

// ==============================================================================
// Types
// ==============================================================================

void NameResolution::visit(const TypeAnnotation_ptr type_node)
{
    std::visit(
        [&](auto& node)
        {
            if constexpr (requires { visit(node); })
            {
                visit(node);
            }
        },
        type_node->data
    );
}

void NameResolution::visit(const TypeAnnotationVector& type_nodes)
{
    for (const auto& type_node : type_nodes)
    {
        visit(type_node);
    }
}

void NameResolution::visit(TypeIdentifierNode& expr)
{
    auto symbol = current_scope->lookup(expr.name);

    Doctor::get().fatal_if_nullptr(
        symbol,
        WaspStage::Semantics,
        "Undefined type: " + expr.name
    );

    expr.symbol = symbol;
}

void NameResolution::visit(ListTypeNode& expr)
{
    visit(expr.element_type);
}

void NameResolution::visit(TupleTypeNode& expr)
{
    visit(expr.element_types);
}

void NameResolution::visit(SetTypeNode& expr)
{
    visit(expr.element_type);
}

void NameResolution::visit(MapTypeNode& expr)
{
    visit(expr.key_type);
    visit(expr.value_type);
}

void NameResolution::visit(VariantTypeNode& expr)
{
    visit(expr.options);
}

void NameResolution::visit(IntersectionTypeNode& expr)
{
    visit(expr.types);
}

void NameResolution::visit(FunctionTypeNode& expr)
{
    visit(expr.input_types);
    visit(expr.return_type);
}

void NameResolution::visit(AngularTypeNode& node)
{
    auto symbol = current_scope->lookup(node.name);

    Doctor::get().fatal_if_nullptr(
        symbol,
        WaspStage::Semantics,
        "Undefined type: " + node.name
    );

    node.symbol = symbol;

    visit(node.type_arguments);
}

} // namespace Wasp
