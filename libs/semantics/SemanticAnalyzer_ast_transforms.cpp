#include "AST.h"
#include "Doctor.h"
#include "Expression.h"
#include "Objects.h"
#include "SemanticAnalyzer.h"
#include "Token.h"
#include "Workspace.h"

#include <cctype>
#include <ctime>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

Object_ptr SemanticAnalyzer::visit(const Expression_ptr expr)
{
    Doctor::get().fatal_if_nullptr(expr, WaspStage::Semantics);

    Object_ptr resolved_type = std::visit(
        [&](auto& node) -> Object_ptr
        {
            if constexpr (requires { this->visit(node); })
            {
                return this->visit(node);
            }
            else
            {
                Doctor::get().fatal(
                    WaspStage::Semantics,
                    "Unhandled Expression"
                );
            }
        },
        expr->data
    );

    // 1. Desugar Literals
    if (expr->is<IntegerLiteral>())
    {
        desugar_literal(expr, "int");
        return get_core_symbol("int")->get_type();
    }
    else if (expr->is<FloatLiteral>())
    {
        desugar_literal(expr, "float");
        return get_core_symbol("float")->get_type();
    }
    else if (expr->is<StringLiteral>())
    {
        desugar_literal(expr, "str");
        return get_core_symbol("str")->get_type();
    }
    else if (expr->is<BooleanLiteral>())
    {
        desugar_literal(expr, "bool");
        return get_core_symbol("bool")->get_type();
    }

    // Desugar Overloaded Operators into standard function calls
    if (expr->is<Infix>())
    {
        auto& infix = expr->as<Infix>();
        if (infix.symbol)
        {
            Symbol_ptr sym = infix.symbol;
            int idx = infix.overload_index;
            std::vector<Expression_ptr> args = {infix.left, infix.right};
            desugar_overloaded_operator(expr, sym, idx, args);
        }
    }
    else if (expr->is<Prefix>())
    {
        auto& prefix = expr->as<Prefix>();
        if (prefix.symbol)
        {
            Symbol_ptr sym = prefix.symbol;
            int idx = prefix.overload_index;
            std::vector<Expression_ptr> args = {prefix.operand};
            desugar_overloaded_operator(expr, sym, idx, args);
        }
    }
    else if (expr->is<Postfix>())
    {
        auto& postfix = expr->as<Postfix>();
        if (postfix.symbol)
        {
            Symbol_ptr sym = postfix.symbol;
            int idx = postfix.overload_index;
            std::vector<Expression_ptr> args = {postfix.operand};
            desugar_overloaded_operator(expr, sym, idx, args);
        }
    }

    return resolved_type;
}

void SemanticAnalyzer::desugar_literal(
    const Expression_ptr& expr,
    const std::string& type_alias_name
)
{
    // 1. Get the compile-time type alias symbol (e.g., "int")
    Symbol_ptr alias_symbol = get_core_symbol(type_alias_name);

    // 2. Unwrap it to find the actual ClassType object
    Object_ptr actual_type = unwrap_type_alias(alias_symbol->get_type());
    auto class_type = actual_type->as<ClassType_ptr>();

    // 3. Look up the actual runtime class symbol by name (e.g., "Int")
    // This works flawlessly now because Pass 1 Hoisting puts 'Int' in the
    // scope!
    Symbol_ptr runtime_class_symbol = current_scope->lookup(class_type->name);

    Doctor::get().fatal_if_nullptr(
        runtime_class_symbol,
        WaspStage::Semantics,
        "Runtime class symbol '" + class_type->name + "' not found."
    );

    // 4. Synthesize the Identifier for the Constructor target
    auto id_node = make_expression(
        Identifier(runtime_class_symbol->name),
        expr->start_token,
        expr->end_token
    );
    id_node->as<Identifier>().symbol = runtime_class_symbol;

    // 5. Move the original literal down into a new child node
    auto literal_child = make_expression(
        std::move(expr->data),
        expr->start_token,
        expr->end_token
    );

    // 6. Transform current node into a Constructor: Int(raw_literal)
    expr->data = Constructor(id_node, {literal_child});
}

void SemanticAnalyzer::desugar_overloaded_operator(
    const Expression_ptr& expr,
    Symbol_ptr operator_symbol,
    int overload_index,
    const std::vector<Expression_ptr>& arguments
)
{
    auto id_node = make_expression(
        Identifier(operator_symbol->name),
        expr->start_token,
        expr->end_token
    );

    bind_identifier(id_node->as<Identifier>(), operator_symbol);

    Call call_node{id_node, arguments};
    call_node.overload_index = overload_index;

    expr->data = std::move(call_node);
}

} // namespace Wasp
