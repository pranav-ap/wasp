#include "SemanticAnalyzer.h"
#include "Doctor.h"
#include "Expression.h"
#include "Objects.h"
#include "Statement.h"
#include "Symbol.h"
#include "SymbolScope.h"
#include "Workspace.h"

#include <cstddef>
#include <ctime>
#include <memory>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#define MAKE_OBJECT_VARIANT(x) std::make_shared<Object>(x)

template <class... Ts> struct overloaded : Ts... {
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp {
// ============================================================================
// ENTRY POINT
// ============================================================================

void SemanticAnalyzer::run(const std::vector<Module_ptr>& build_order) {
    enter_scope(ScopeType::WORKSPACE);
    register_natives();

    for (const auto& module : build_order) {
        enter_scope(ScopeType::MODULE);

        // Push the hoisted exports into this module's scope
        for (const auto& [name, symbol] : module->exports) {
            current_scope->define(symbol);
        }

        visit(module->block);

        leave_scope();
    }

    leave_scope();
}

void SemanticAnalyzer::register_natives() {
    std::unordered_map<std::string, int> native_names = native_registry->get_all_native_names();

    for (const auto& [name, index] : native_names) {
        auto symbol_type = native_registry->get_native_object_type(index);

        auto symbol = SymbolFactory::create_function(name, symbol_type, true);
        current_scope->define(symbol);
    }
}

// ============================================================================
// SCOPE MANAGEMENT
// ============================================================================

void SemanticAnalyzer::enter_scope(ScopeType scope_type) {
    current_scope = std::make_shared<SymbolScope>(scope_type, current_scope);
}

void SemanticAnalyzer::leave_scope() {
    if (current_scope) {
        current_scope = current_scope->get_enclosing();
    }
}

// ============================================================================
// High Level Visitors
// ============================================================================

void SemanticAnalyzer::visit(std::vector<Statement_ptr>& statements) {
    for (const auto& stmt : statements) {
        visit(stmt);
    }
}

void SemanticAnalyzer::visit(const Statement_ptr statement) {
    Doctor::get().fatal_if_nullptr(statement, WaspStage::Semantics);

    std::visit(
        overloaded{
            [&](ExpressionStatement& stat) { visit(stat); },
            [&](VariableDefinition& stat) { visit(stat); },
            [&](AliasDefinition& stat) { visit(stat); },
            [&](EnumDefinition& stat) { visit(stat); },
            [&](FunctionDefinition& stat) { visit(stat); },
            [&](ClassDefinition& stat) { visit(stat); },
            [&](TraitDefinition& stat) { visit(stat); },
            [&](ImplDefinition& stat) { visit(stat); },
            [&](AnnotationDefinition& stat) { visit(stat); },
            [&](IfBranch& stat) { visit(stat); },
            [&](ElseBranch& stat) { visit(stat); },
            [&](SimpleLoop& stat) { visit(stat); },
            [&](ForInLoop& stat) { visit(stat); },
            [&](LoopControl& stat) { visit(stat); },
            [&](Pass& stat) { visit(stat); },
            [&](Return& stat) { visit(stat); },
            [&](SimpleImport& stat) { visit(stat); },
            [&](FromImport& stat) { visit(stat); },
            [](auto) {
                Doctor::get().Doctor::get().fatal(
                    WaspStage::Semantics, "Unhandled Statement in Semantic Analyzer!"
                );
            }
        },
        statement->data
    );
}

void SemanticAnalyzer::visit(ExpressionStatement& statement) { visit(statement.expression); }

ObjectVector SemanticAnalyzer::visit(ExpressionVector expressions) {
    ObjectVector computed_types;
    computed_types.reserve(expressions.size());

    for (const auto& expr : expressions) {
        computed_types.push_back(visit(expr));
    }

    return computed_types;
}

Object_ptr SemanticAnalyzer::visit(const Expression_ptr expr) {
    Doctor::get().fatal_if_nullptr(expr, WaspStage::Semantics);

    return std::visit(
        overloaded{
            // Primitives
            [&](int& node) -> Object_ptr { return visit(node); },
            [&](double& node) -> Object_ptr { return visit(node); },
            [&](std::string& node) -> Object_ptr { return visit(node); },
            [&](bool& node) -> Object_ptr { return visit(node); },

            // Identifiers & Access
            [&](Identifier& node) -> Object_ptr { return visit(node); },
            [&](DotLiteral& node) -> Object_ptr { return visit(node); },

            // Operators
            [&](Prefix& node) -> Object_ptr { return visit(node); },
            [&](Infix& node) -> Object_ptr { return visit(node); },
            [&](Postfix& node) -> Object_ptr { return visit(node); },

            // Collections
            [&](ListLiteral& node) -> Object_ptr { return visit(node); },
            [&](TupleLiteral& node) -> Object_ptr { return visit(node); },
            [&](MapLiteral& node) -> Object_ptr { return visit(node); },
            [&](SetLiteral& node) -> Object_ptr { return visit(node); },
            [&](RangeLiteral& node) -> Object_ptr { return visit(node); },

            // Variables & Assignments
            [&](VariableDefinitionExpression& node) -> Object_ptr { return visit(node); },
            [&](UntypedAssignment& node) -> Object_ptr { return visit(node); },
            [&](TypedAssignment& node) -> Object_ptr { return visit(node); },
            [&](TypePattern& node) -> Object_ptr { return visit(node); },

            // Control Flow
            [&](IfTernaryBranch& node) -> Object_ptr { return visit(node); },
            [&](ElseTernaryBranch& node) -> Object_ptr { return visit(node); },
            [&](Call& node) -> Object_ptr { return visit(node); },

            // Fallback
            [](auto&) -> Object_ptr {
                Doctor::get().Doctor::get().fatal(
                    WaspStage::Semantics, "Unhandled Expression in Semantic Analyzer!"
                );
            }
        },
        expr->data
    );
}

// ============================================================================
// Branching
// ============================================================================

Object_ptr SemanticAnalyzer::visit(IfTernaryBranch& expr) {
    enter_scope(ScopeType::BRANCH);

    Object_ptr cond_type = visit(expr.test);
    type_system->expect_condition_type(current_scope, cond_type);

    Object_ptr then_type = visit(expr.true_expression);
    leave_scope();

    if (expr.alternative) {
        Object_ptr else_type = visit(expr.alternative);
        ObjectVector unique_types =
            type_system->remove_duplicates(current_scope, {then_type, else_type});

        if (unique_types.size() == 1) {
            return unique_types[0];
        }

        return MAKE_OBJECT_VARIANT(VariantType(unique_types));
    }

    Object_ptr none_type = MAKE_OBJECT_VARIANT(NoneType());
    ObjectVector unique_types =
        type_system->remove_duplicates(current_scope, {then_type, none_type});

    if (unique_types.size() == 1)
        return unique_types[0];

    return MAKE_OBJECT_VARIANT(VariantType(unique_types));
}

void SemanticAnalyzer::visit(IfBranch& statement) {
    enter_scope(ScopeType::BRANCH);

    Object_ptr cond_type = visit(statement.test);
    type_system->expect_condition_type(current_scope, cond_type);

    visit(statement.body);
    leave_scope();

    if (statement.alternative) {
        visit(*statement.alternative);
    }
}

Object_ptr SemanticAnalyzer::visit(ElseTernaryBranch& expr) {
    enter_scope(ScopeType::BRANCH);
    Object_ptr type = visit(expr.expression);
    leave_scope();

    return type;
}

void SemanticAnalyzer::visit(ElseBranch& statement) {
    enter_scope(ScopeType::BRANCH);
    visit(statement.body);
    leave_scope();
}

void SemanticAnalyzer::visit(Pass& statement) {}

// ---------------------------------------------------------------------------
// Loops
// ---------------------------------------------------------------------------

void SemanticAnalyzer::visit(SimpleLoop& statement) {
    visit(statement.condition);
    enter_scope(ScopeType::LOOP);
    visit(statement.body);
    leave_scope();
}

void SemanticAnalyzer::visit(ForInLoop& loop_stmt) {
    Object_ptr iterable_type = visit(loop_stmt.iterable_expression);
    type_system->expect_iterable_type(current_scope, iterable_type);

    Object_ptr element_type = MAKE_OBJECT_VARIANT(AnyType());

    enter_scope(ScopeType::LOOP);

    Doctor::get().assert(
        loop_stmt.lhs->is<Identifier>(),
        WaspStage::Semantics,
        "For-in loop variable must be an identifier."
    );

    auto& identifier_node = loop_stmt.lhs->as<Identifier>();
    std::string symbol_name = identifier_node.name;

    auto loop_variable_symbol = SymbolFactory::create_variable(
        symbol_name,
        element_type,
        loop_stmt.lhs_is_mutable,
        false, // is_captured
        current_scope->get_closure_depth(),
        current_scope->get_lexical_depth()
    );

    current_scope->define(loop_variable_symbol);
    identifier_node.symbol = loop_variable_symbol;

    visit(loop_stmt.body);

    leave_scope();
}

void SemanticAnalyzer::visit(LoopControl& statement) {
    SymbolScope_ptr scope = current_scope;

    Doctor::get().assert(
        scope->enclosed_in(ScopeType::LOOP),
        WaspStage::Semantics,
        "Loop control statement ('break', 'continue', 'redo') must be inside a loop."
    );
}

// --------------------------------------------------------------------------
// Definitions
// --------------------------------------------------------------------------

void SemanticAnalyzer::visit(AliasDefinition& statement) {}

void SemanticAnalyzer::visit(EnumDefinition& statement) {}

void SemanticAnalyzer::visit(ClassDefinition& statement) {}

void SemanticAnalyzer::visit(TraitDefinition& statement) {}

void SemanticAnalyzer::visit(ImplDefinition& statement) {}

void SemanticAnalyzer::visit(AnnotationDefinition& statement) {}

// ========================================================================
// Imports Visitors
// ========================================================================

void SemanticAnalyzer::visit(SimpleImport& import_stmt) {
    auto mod = workspace->get_module(import_stmt.resolved_path);
    Doctor::get().fatal_if_nullptr(mod, WaspStage::Semantics);

    // Extract "math" from stuff.math
    std::string module_name = import_stmt.resolved_path.stem().string();

    // If the user provided an alias, use that instead
    if (import_stmt.alias.has_value()) {
        module_name = import_stmt.alias.value();
    }

    auto namespace_symbol = SymbolFactory::create_module(module_name);
    current_scope->define(namespace_symbol);
}

void SemanticAnalyzer::visit(FromImport& import_stmt) {
    auto mod = workspace->get_module(import_stmt.resolved_path);
    Doctor::get().fatal_if_nullptr(mod, WaspStage::Semantics);

    // `from math import sqrt, pi as pie`

    for (const auto& sym : import_stmt.symbols) {

        // Check if the dependency actually exports the requested name
        auto it = mod->exports.find(sym.name);

        Doctor::get().assert(
            it != mod->exports.end(),
            WaspStage::Semantics,
            "Import Error: Module '" + import_stmt.resolved_path.stem().string() +
                "' does not export '" + sym.name + "'."
        );

        Symbol_ptr symbol_to_define = it->second;

        if (sym.alias.has_value()) {
            symbol_to_define = std::make_shared<Symbol>(*it->second);
            symbol_to_define->name = sym.alias.value();
        }

        current_scope->define(symbol_to_define);
    }
}

// ---------------------------------------------------------------------------
// Variable Definitions & Assignments
// --------------------------------------------------------------------------

void SemanticAnalyzer::visit(VariableDefinition& statement) {
    define_variable(statement.expression, statement.is_mutable);
}

Object_ptr SemanticAnalyzer::visit(VariableDefinitionExpression& expr) {
    return define_variable(expr.assignment, expr.is_mutable);
}

Object_ptr SemanticAnalyzer::visit(UntypedAssignment& expr) {
    return mutate_variable(expr.lhs_expression, expr.rhs_expression);
}

Object_ptr SemanticAnalyzer::visit(TypedAssignment& expr) {
    Doctor::get().fatal(
        WaspStage::Semantics, "Internal Semantic Error: TypedAssignment visited directly."
    );

    return nullptr;
}

Object_ptr SemanticAnalyzer::define_variable(Expression_ptr assignment_node, bool is_mutable) {
    Expression_ptr identifier_expr = nullptr;
    Expression_ptr initializer_expr = nullptr;
    Object_ptr declared_type = nullptr;

    std::visit(
        overloaded{
            [&](UntypedAssignment& assign) {
                identifier_expr = assign.lhs_expression;
                initializer_expr = assign.rhs_expression;
            },
            [&](TypedAssignment& assign) {
                identifier_expr = assign.lhs_expression;
                initializer_expr = assign.rhs_expression;
                declared_type = visit(assign.type_node);
            },
            [](auto&) {
                Doctor::get().fatal(WaspStage::Semantics, "Invalid variable definition expression");
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

    // Resolve RHS Type
    Object_ptr initializer_type = visit(initializer_expr);
    Object_ptr resolved_type = initializer_type;

    // Check whether it is assignable
    if (declared_type) {
        Doctor::get().assert(
            type_system->assignable(current_scope, declared_type, initializer_type),
            WaspStage::Semantics,
            "Type mismatch in variable definition for '" + symbol_name + "'."
        );

        resolved_type = declared_type;
    }

    // Hoister Usage

    if (Symbol_ptr hoisted_symbol = current_scope->lookup(symbol_name)) {
        // If it exists, it must be a hoisted global waiting for its type.
        Doctor::get().assert(
            hoisted_symbol->get_type() == nullptr,
            WaspStage::Semantics,
            "Variable '" + symbol_name + "' is already defined in the current scope."
        );

        hoisted_symbol->set_type(resolved_type);
        identifier_expr->as<Identifier>().symbol = hoisted_symbol;

        return hoisted_symbol->get_type();
    }

    // New Local Variable

    auto local_symbol = SymbolFactory::create_variable(
        symbol_name,
        resolved_type,
        is_mutable,
        false,
        current_scope->get_closure_depth(),
        current_scope->get_lexical_depth()
    );

    current_scope->define(local_symbol);
    identifier_expr->as<Identifier>().symbol = local_symbol;

    return local_symbol->get_type();
}

Object_ptr
SemanticAnalyzer::mutate_variable(Expression_ptr identifier_expr, Expression_ptr assigned_expr) {
    Doctor::get().assert(
        identifier_expr->is<Identifier>(),
        WaspStage::Semantics,
        "Left-hand side of assignment must be an Identifier."
    );

    auto& identifier_node = identifier_expr->as<Identifier>();
    std::string symbol_name = identifier_node.name;

    Symbol_ptr target_symbol = current_scope->lookup(symbol_name);

    Doctor::get().assert(
        target_symbol != nullptr,
        WaspStage::Semantics,
        "Cannot assign to undefined variable '" + symbol_name + "'."
    );

    Doctor::get().assert(
        target_symbol->is<VariableData>(),
        WaspStage::Semantics,
        "Cannot assign to non-variable symbol '" + symbol_name + "'."
    );

    auto& var_data = target_symbol->as<VariableData>();

    Doctor::get().assert(
        var_data.is_mutable,
        WaspStage::Semantics,
        "Cannot reassign immutable variable '" + symbol_name + "'."
    );

    if (target_symbol->should_be_captured(current_scope->get_closure_depth())) {
        var_data.is_captured = true;
    }

    identifier_node.symbol = target_symbol;

    // Type Checking
    Object_ptr assigned_type = visit(assigned_expr);

    Doctor::get().assert(
        type_system->assignable(current_scope, target_symbol->get_type(), assigned_type),
        WaspStage::Semantics,
        "Type mismatch in assignment to '" + symbol_name + "'."
    );

    return target_symbol->get_type();
}

Object_ptr SemanticAnalyzer::visit(Identifier& expr) {
    Symbol_ptr target_symbol = current_scope->lookup(expr.name);
    Doctor::get().fatal_if_nullptr(target_symbol, WaspStage::Semantics);

    // Only variables need to be captured by closures
    if (target_symbol->is<VariableData>()) {
        auto& var_data = target_symbol->as<VariableData>();
        target_symbol->capture_if_required(current_scope->get_closure_depth());
    }

    expr.symbol = target_symbol;

    return target_symbol->get_type();
}

Object_ptr SemanticAnalyzer::visit(DotLiteral& expr) { return nullptr; }

// ============================================================================
// Functions & Calls
// ============================================================================

void SemanticAnalyzer::visit(FunctionDefinition& func_def) {
    Object_ptr resolved_return_type =
        func_def.return_type ? visit(func_def.return_type) : MAKE_OBJECT_VARIANT(NoneType());

    ObjectVector parameter_types;
    std::vector<std::string> parameter_names;

    // Resolve Parameter Types
    for (const auto& [param_name, type_annotation] : func_def.parameters) {
        Object_ptr resolved_param_type = MAKE_OBJECT_VARIANT(AnyType());

        if (type_annotation) {
            resolved_param_type = visit(type_annotation);
        }

        parameter_names.push_back(param_name);
        parameter_types.push_back(resolved_param_type);
    }

    auto function_signature =
        MAKE_OBJECT_VARIANT(FunctionType(parameter_types, resolved_return_type));

    // Use the Hoister
    if (Symbol_ptr hoisted_symbol = current_scope->lookup(func_def.name)) {
        // If it exists, it must be a hoisted global waiting for its resolved signature
        Doctor::get().assert(
            hoisted_symbol->get_type() == nullptr,
            WaspStage::Semantics,
            "Function '" + func_def.name + "' is already defined in this scope."
        );

        hoisted_symbol->set_type(function_signature);
        func_def.symbol = hoisted_symbol;

    } else {
        // It's a nested (local) function, so we create a new symbol for it
        auto local_symbol = SymbolFactory::create_function(
            func_def.name,
            function_signature,
            false, // is_native
            current_scope->get_closure_depth(),
            current_scope->get_lexical_depth()
        );

        current_scope->define(local_symbol);
        func_def.symbol = local_symbol;
    }

    enter_scope(ScopeType::FUNCTION);
    return_type_stack.push_back(resolved_return_type);

    // Define Parameters as Local Variables in the New Scope
    for (size_t i = 0; i < parameter_names.size(); ++i) {
        auto param_symbol = SymbolFactory::create_variable(
            parameter_names[i],
            parameter_types[i],
            false,
            false,
            current_scope->get_closure_depth(),
            current_scope->get_lexical_depth()
        );

        current_scope->define(param_symbol);
    }

    visit(func_def.body);

    return_type_stack.pop_back();
    leave_scope();
}

Object_ptr SemanticAnalyzer::visit(Call& call_expr) {
    // Resolve the Callee Type
    Object_ptr callee_type = visit(call_expr.callee);

    Doctor::get().assert(
        callee_type->is<FunctionType>(),
        WaspStage::Semantics,
        "Expression is not callable. Type is: " + Wasp::stringify_object(callee_type)
    );

    if (call_expr.callee->is<Identifier>()) {
        call_expr.symbol = call_expr.callee->as<Identifier>().symbol;
    }

    const auto& function_signature = callee_type->as<FunctionType>();

    // Check Argument Types

    Doctor::get().assert(
        call_expr.arguments.size() == function_signature.input_types.size(),
        WaspStage::Semantics,
        "Incorrect number of arguments. Expected " +
            std::to_string(function_signature.input_types.size()) + ", got " +
            std::to_string(call_expr.arguments.size()) + "."
    );

    for (size_t i = 0; i < call_expr.arguments.size(); ++i) {
        Object_ptr argument_type = visit(call_expr.arguments[i]);
        Object_ptr parameter_type = function_signature.input_types[i];

        Doctor::get().assert(
            type_system->assignable(current_scope, parameter_type, argument_type),
            WaspStage::Semantics,
            "Argument mismatch at index " + std::to_string(i) + ". Expected " +
                Wasp::stringify_object(parameter_type) + ", got " +
                Wasp::stringify_object(argument_type) + "."
        );
    }

    if (function_signature.return_type.has_value()) {
        return function_signature.return_type.value();
    }

    return MAKE_OBJECT_VARIANT(NoneType());
}

void SemanticAnalyzer::visit(Return& statement) {
    Doctor::get().assert(
        !return_type_stack.empty(),
        WaspStage::Semantics,
        "'return' statement used outside of a function."
    );

    Object_ptr expected_type = return_type_stack.back();
    Object_ptr actual_type = statement.expression.has_value() ? visit(statement.expression.value())
                                                              : MAKE_OBJECT_VARIANT(NoneType());

    Doctor::get().assert(
        type_system->assignable(current_scope, expected_type, actual_type),
        WaspStage::Semantics,
        "Return type mismatch. Expected " + Wasp::stringify_object(expected_type) + ", got " +
            Wasp::stringify_object(actual_type)
    );
}

// ============================================================================
// EXPRESSION
// ============================================================================

Object_ptr SemanticAnalyzer::visit(int expr) { return MAKE_OBJECT_VARIANT(IntType()); }

Object_ptr SemanticAnalyzer::visit(double expr) { return MAKE_OBJECT_VARIANT(FloatType()); }

Object_ptr SemanticAnalyzer::visit(std::string expr) { return MAKE_OBJECT_VARIANT(StringType()); }

Object_ptr SemanticAnalyzer::visit(bool expr) { return MAKE_OBJECT_VARIANT(BooleanType()); }

// --- Operators ---

Object_ptr SemanticAnalyzer::visit(Prefix& expr) {
    Object_ptr right_type = visit(expr.operand);
    return type_system->infer(current_scope, right_type, expr.op.type);
}

Object_ptr SemanticAnalyzer::visit(Infix& expr) {
    Object_ptr left_type = visit(expr.left);
    Object_ptr right_type = visit(expr.right);
    return type_system->infer(current_scope, left_type, expr.op.type, right_type);
}

Object_ptr SemanticAnalyzer::visit(Postfix& expr) {
    Object_ptr left_type = visit(expr.operand);
    type_system->expect_number_type(left_type);
    return left_type;
}

Object_ptr SemanticAnalyzer::visit(ListLiteral& expr) {
    if (expr.expressions.empty())
        return MAKE_OBJECT_VARIANT(ListType(MAKE_OBJECT_VARIANT(AnyType())));

    ObjectVector element_types;
    for (const auto& element : expr.expressions)
        element_types.push_back(visit(element));

    ObjectVector unique_types = type_system->remove_duplicates(current_scope, element_types);

    if (unique_types.size() == 1)
        return MAKE_OBJECT_VARIANT(ListType(unique_types[0]));
    return MAKE_OBJECT_VARIANT(ListType(MAKE_OBJECT_VARIANT(VariantType(unique_types))));
}

Object_ptr SemanticAnalyzer::visit(TupleLiteral& expr) {
    ObjectVector element_types;
    for (const auto& element : expr.expressions)
        element_types.push_back(visit(element));
    return MAKE_OBJECT_VARIANT(TupleType(element_types));
}

Object_ptr SemanticAnalyzer::visit(MapLiteral& expr) {
    if (expr.pairs.empty())
        return MAKE_OBJECT_VARIANT(
            MapType(MAKE_OBJECT_VARIANT(AnyType()), MAKE_OBJECT_VARIANT(AnyType()))
        );

    ObjectVector key_types;
    ObjectVector val_types;

    for (const auto& [key_expr, val_expr] : expr.pairs) {
        Object_ptr k_type = visit(key_expr);
        type_system->expect_key_type(current_scope, k_type);
        key_types.push_back(k_type);
        val_types.push_back(visit(val_expr));
    }

    ObjectVector unique_keys = type_system->remove_duplicates(current_scope, key_types);
    ObjectVector unique_vals = type_system->remove_duplicates(current_scope, val_types);

    Object_ptr final_key_type =
        unique_keys.size() == 1 ? unique_keys[0] : MAKE_OBJECT_VARIANT(VariantType(unique_keys));
    Object_ptr final_val_type =
        unique_vals.size() == 1 ? unique_vals[0] : MAKE_OBJECT_VARIANT(VariantType(unique_vals));

    return MAKE_OBJECT_VARIANT(MapType(final_key_type, final_val_type));
}

Object_ptr SemanticAnalyzer::visit(SetLiteral& expr) {
    if (expr.expressions.empty())
        return MAKE_OBJECT_VARIANT(SetType(MAKE_OBJECT_VARIANT(AnyType())));

    ObjectVector element_types;
    for (const auto& element : expr.expressions) {
        Object_ptr el_type = visit(element);
        type_system->expect_key_type(current_scope, el_type);
        element_types.push_back(el_type);
    }

    ObjectVector unique_types = type_system->remove_duplicates(current_scope, element_types);

    if (unique_types.size() == 1)
        return MAKE_OBJECT_VARIANT(SetType(unique_types[0]));
    return MAKE_OBJECT_VARIANT(SetType(MAKE_OBJECT_VARIANT(VariantType(unique_types))));
}

Object_ptr SemanticAnalyzer::visit(RangeLiteral& expr) {
    Object_ptr start_type = nullptr;
    Object_ptr end_type = nullptr;

    if (expr.start) {
        start_type = visit(expr.start);
        type_system->expect_number_type(start_type);
    }

    if (expr.end) {
        end_type = visit(expr.end);
        type_system->expect_number_type(end_type);
    }

    if (type_system->is_float_type(start_type) || type_system->is_float_type(end_type)) {
        return MAKE_OBJECT_VARIANT(ListType(MAKE_OBJECT_VARIANT(FloatType())));
    }

    return MAKE_OBJECT_VARIANT(ListType(MAKE_OBJECT_VARIANT(IntType())));
}

Object_ptr SemanticAnalyzer::visit(TypePattern& expr) { return nullptr; }
} // namespace Wasp
