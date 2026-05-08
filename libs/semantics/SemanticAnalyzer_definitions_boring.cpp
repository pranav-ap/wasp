#include <functional>
#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "AST.h"
#include "Doctor.h"
#include "Expression.h"
#include "Objects.h"
#include "SemanticAnalyzer.h"
#include "Statement.h"
#include "SymbolScope.h"
#include "Workspace.h"

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

// ---------------------------------------------------------------------------
// Variable Definitions & Assignments
// ---------------------------------------------------------------------------

Object_ptr SemanticAnalyzer::define_variable(
    Expression_ptr assignment_node,
    bool is_mutable
)
{
    Expression_ptr identifier_expr = nullptr;
    Expression_ptr rhs_expr = nullptr;
    Object_ptr declared_type = nullptr;

    std::visit(
        overloaded{
            [&](UntypedAssignment& assign)
            {
                identifier_expr = assign.lhs_expression;
                rhs_expr = assign.rhs_expression;
            },
            [&](TypedAssignment& assign)
            {
                identifier_expr = assign.lhs_expression;
                rhs_expr = assign.rhs_expression;
                declared_type = visit(assign.type_node);
            },
            [](auto&)
            {
                Doctor::get().fatal(
                    WaspStage::Semantics,
                    "Invalid variable definition expression"
                );
            }
        },
        assignment_node->data
    );

    Doctor::get().assert(
        identifier_expr->is<Identifier>(),
        WaspStage::Semantics,
        "Left-hand side of definition must be an Identifier."
    );

    std::string symbol_name = identifier_expr->as<Identifier>().name;

    Object_ptr initializer_type = visit(rhs_expr);

    auto promote = [&](Object_ptr native_type, const std::string& alias_name)
    {
        if (declared_type && declared_type == native_type)
        {
            return native_type;
        }

        // 2. Otherwise, look up the global class wrapper (e.g., `int` -> `Int`)
        if (auto wrapper_sym = current_scope->lookup(alias_name)->resolve())
        {
            return unwrap_type(wrapper_sym->get_type());
        }

        // Fallback if the standard library isn't loaded
        return native_type;
    };

    if (initializer_type->is<IntLiteralType>())
    {
        initializer_type = promote(
            workspace->pool->get_native_int_type(),
            "int"
        );
    }
    else if (initializer_type->is<FloatLiteralType>())
    {
        initializer_type = promote(
            workspace->pool->get_native_float_type(),
            "float"
        );
    }
    else if (initializer_type->is<StringLiteralType>())
    {
        initializer_type = promote(
            workspace->pool->get_native_string_type(),
            "str"
        );
    }
    else if (initializer_type->is<BooleanLiteralType>())
    {
        initializer_type = promote(workspace->pool->get_boolean_type(), "bool");
    }

    Object_ptr resolved_type = initializer_type;

    if (declared_type)
    {
        Doctor::get().assert(
            type_system
                ->assignable(current_scope, declared_type, initializer_type),
            WaspStage::Semantics,
            "Type mismatch in variable definition for " + symbol_name
        );

        resolved_type = declared_type;
    }

    if (Symbol_ptr hoisted_symbol = current_scope->lookup(symbol_name))
    {
        Doctor::get().assert(
            hoisted_symbol->get_type() == nullptr,
            WaspStage::Semantics,
            "Variable '" + symbol_name + "' is hoisted but already has a type!"
        );

        hoisted_symbol->set_type(resolved_type);
        identifier_expr->as<Identifier>().symbol = hoisted_symbol;

        return hoisted_symbol->get_type();
    }

    auto local_symbol = SymbolFactory::create_variable(
        symbol_name,
        resolved_type,
        is_mutable,
        current_scope->get_closure_depth(),
        current_scope->get_lexical_depth()
    );

    current_scope->define(local_symbol);
    identifier_expr->as<Identifier>().symbol = local_symbol;

    return local_symbol->get_type();
}

void SemanticAnalyzer::validate_purity_constraints(Symbol_ptr target_symbol) const
{
    if (target_symbol->closure_depth >= current_scope->get_closure_depth())
    {
        return;
    }

    auto scope = current_scope;

    while (scope != nullptr &&
           scope->get_closure_depth() > target_symbol->closure_depth)
    {
        if (scope->get_type() == ScopeType::PURE_FUNCTION ||
            scope->get_type() == ScopeType::PURE_METHOD)
        {
            Doctor::get().fatal(
                WaspStage::Semantics,
                "Pure functions cannot mutate outer state variable '" +
                    target_symbol->name + "'"
            );
        }

        scope = scope->get_enclosing();
    }
}

Object_ptr SemanticAnalyzer::mutate_variable(
    Expression_ptr identifier_expr,
    Expression_ptr assigned_expr
)
{
    Doctor::get().assert(
        identifier_expr->is<Identifier>(),
        WaspStage::Semantics,
        "Left-hand side of assignment must be an Identifier."
    );

    auto& identifier_node = identifier_expr->as<Identifier>();
    std::string symbol_name = identifier_node.name;

    Symbol_ptr target_symbol = current_scope->lookup(symbol_name);

    Doctor::get().fatal_if_nullptr(
        target_symbol,
        WaspStage::Semantics,
        "Cannot assign to undefined variable '" + symbol_name + "'"
    );

    Doctor::get().assert(
        target_symbol->payload_is<VariableData>(),
        WaspStage::Semantics,
        "Cannot assign to non-variable symbol '" + symbol_name + "'"
    );

    auto& var_data = target_symbol->get_payload_as<VariableData>();

    Doctor::get().assert(
        var_data.is_mutable,
        WaspStage::Semantics,
        "Cannot reassign immutable variable '" + symbol_name + "'"
    );

    validate_purity_constraints(target_symbol);

    identifier_node.symbol = target_symbol;

    Object_ptr assigned_type = visit(assigned_expr);

    Doctor::get().assert(
        type_system
            ->assignable(current_scope, target_symbol->get_type(), assigned_type),
        WaspStage::Semantics,
        "Type mismatch in assignment to '" + symbol_name + "'"
    );

    return target_symbol->get_type();
}

Object_ptr SemanticAnalyzer::mutate_member(
    Expression_ptr lhs_expr,
    Expression_ptr rhs_expr
)
{
    auto& mac = lhs_expr->as<MemberAccess>();

    Object_ptr expected_type = visit(mac);

    if (mac.member_index == -1)
    {
        auto symbol = mac.right->as<Identifier>().symbol;
        if (symbol && symbol->payload_is<VariableData>())
        {
            Doctor::get().assert(
                symbol->get_payload_as<VariableData>().is_mutable,
                WaspStage::Semantics,
                "Cannot reassign immutable shared member: " + symbol->name
            );
        }
    }

    Object_ptr actual_type = visit(rhs_expr);

    Doctor::get().assert(
        type_system->assignable(current_scope, expected_type, actual_type),
        WaspStage::Semantics,
        "Type mismatch in member assignment."
    );

    return expected_type;
}

void SemanticAnalyzer::visit(VariableDefinition& statement)
{
    define_variable(statement.expression, statement.is_mutable);
}

Object_ptr SemanticAnalyzer::visit(VariableDefinitionExpression& expr)
{
    return define_variable(expr.assignment, expr.is_mutable);
}

Object_ptr SemanticAnalyzer::visit(UntypedAssignment& expr)
{
    if (expr.lhs_expression->is<Identifier>())
    {
        return mutate_variable(expr.lhs_expression, expr.rhs_expression);
    }

    if (expr.lhs_expression->is<MemberAccess>())
    {
        return mutate_member(expr.lhs_expression, expr.rhs_expression);
    }

    Doctor::get().fatal(
        WaspStage::Semantics,
        "Left-hand side of assignment must be an Identifier or MemberAccess."
    );
}

Object_ptr SemanticAnalyzer::visit(TypedAssignment& expr)
{
    Doctor::get().fatal(
        WaspStage::Semantics,
        "TypedAssignment cannot be visited directly"
    );
}

// ---------------------------------------------------------------------------
// Others
// ---------------------------------------------------------------------------

void SemanticAnalyzer::visit(TypeAliasDefinition& def)
{
    auto type_alias_obj = def.symbol->get_type();
    Doctor::get().fatal_if_nullptr(
        type_alias_obj,
        WaspStage::Semantics,
        "Type alias symbol has no type"
    );

    auto type_alias = type_alias_obj->as<TypeAlias_ptr>();
    bool has_generics = !def.generics.empty();

    if (has_generics)
    {
        enter_scope(ScopeType::TEMPLATE);
        for (const auto& [name, generic_type] : type_alias->generics)
        {
            current_scope->define(SymbolFactory::create_generic(name, generic_type));
        }
    }

    Object_ptr aliased_type = visit(def.ref_type);
    type_alias->underlying_type = aliased_type;

    if (has_generics)
    {
        leave_scope();
    }
}

void SemanticAnalyzer::visit(EnumDefinition& def)
{
    int global_enum_value = 0;

    std::function<EnumType_ptr(const EnumDefinition&, const std::string&)>
        build_enum = [&](const EnumDefinition& e_def,
                         const std::string& prefix) -> EnumType_ptr
    {
        std::string current_name = prefix.empty() ? e_def.name
                                                  : prefix + "." + e_def.name;
        auto enum_type = std::make_shared<EnumType>(current_name);

        for (const auto& [name, old_val] : e_def.members)
        {
            enum_type->members[current_name + "." + name] = global_enum_value++;
        }

        for (const auto& nested_def : e_def.nested_enums)
        {
            enum_type
                ->nested_enums[current_name + "." + nested_def.name] = build_enum(
                nested_def,
                current_name
            );
        }

        return enum_type;
    };

    auto enum_type = build_enum(def, "");
    def.symbol->set_type(make_object(enum_type));
}

void SemanticAnalyzer::visit(Return& statement)
{
    Doctor::get().assert(
        !return_type_stack.empty(),
        WaspStage::Semantics,
        "'return' statement used outside of a function."
    );

    Object_ptr expected = return_type_stack.back();
    Object_ptr actual = statement.expression ? visit(statement.expression.value())
                                             : workspace->pool->get_none_type();

    Doctor::get().assert(
        type_system->assignable(current_scope, expected, actual),
        WaspStage::Semantics,
        "Return type mismatch"
    );
}

void SemanticAnalyzer::visit(Native& statement)
{
    Doctor::get().fatal_if_nullptr(
        current_module,
        WaspStage::Semantics,
        "Current module is nullptr while analyzing native statement"
    );

    std::string path = current_module->absolute_filepath.generic_string();

    Doctor::get().assert(
        path.find("/libs/core/") != std::string::npos,
        WaspStage::Semantics,
        "The 'native' keyword is strictly reserved for internal core libraries."
    );
}

void SemanticAnalyzer::visit(AnnotationDefinition& statement)
{
}

void SemanticAnalyzer::visit(FieldDefinition& stat)
{
    Doctor::get().fatal(
        WaspStage::Semantics,
        "Fields cannot be defined outside of a class."
    );
}

void SemanticAnalyzer::visit(MethodDefinition& stat)
{
    Doctor::get().fatal(
        WaspStage::Semantics,
        "Methods cannot be defined outside of a class or trait."
    );
}

} // namespace Wasp
