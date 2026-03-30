#include "AST.h"
#include "Compiler.h"
#include "Doctor.h"
#include "Expression.h"
#include "OpCode.h"
#include "Statement.h"

#include <memory>
#include <string>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

// -----------------------------------------------------------------------
// Variable Definition
// -----------------------------------------------------------------------

void Compiler::visit(VariableDefinition& statement)
{
    compile_variable_definition(statement.expression, false);
}

void Compiler::visit(VariableDefinitionExpression& expr)
{
    compile_variable_definition(expr.assignment, true);
}

void Compiler::compile_variable_definition(const Expression_ptr& assignment, bool as_expression)
{
    Doctor::get().fatal_if_nullptr(assignment, WaspStage::Compiler);

    Expression_ptr lhs = nullptr;
    Expression_ptr rhs = nullptr;

    if (assignment->is<UntypedAssignment>())
    {
        const auto& assign = assignment->as<UntypedAssignment>();
        lhs = assign.lhs_expression;
        rhs = assign.rhs_expression;
    }
    else if (assignment->is<TypedAssignment>())
    {
        const auto& assign = assignment->as<TypedAssignment>();
        lhs = assign.lhs_expression;
        rhs = assign.rhs_expression;
    }
    else
    {
        Doctor::get().fatal(WaspStage::Compiler, "Invalid definition assignment type");
    }

    visit(rhs);

    Doctor::get().assert(
        lhs->is<Identifier>(),
        WaspStage::Compiler,
        "Left-hand side of definition must be an Identifier"
    );

    auto symbol = lhs->as<Identifier>().symbol;
    Doctor::get().fatal_if_nullptr(symbol, WaspStage::Compiler);

    locals.push_back(symbol);
}

} // namespace Wasp
