#include "SemanticAnalyzer.h"
#include "Expression.h"
#include "Objects.h"
#include "Statement.h"
#include "Symbol.h"
#include "SymbolScope.h"

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#ifndef ASSERT
#include <cassert>
#define ASSERT(condition, message) assert((condition) && message)
#endif

#define MAKE_OBJECT_VARIANT(x) std::make_shared<Object>(x)
#define FATAL(message) throw std::runtime_error(message)
#define NULL_CHECK(x) ASSERT(x != nullptr, "Oh shit! A nullptr")

template <class... Ts>
struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{
    // ============================================================================
    // ENTRY POINT
    // ============================================================================

    void SemanticAnalyzer::run(struct Module &ast)
    { 
        enter_scope(ScopeType::WORKSPACE);
        register_natives();

        enter_scope(ScopeType::MODULE);
        visit(ast.statements);
        leave_scope();
        
        leave_scope();
    }

    void SemanticAnalyzer::register_natives()
    {
        std::unordered_map<std::string, int> native_names = native_registry->get_all_native_names();

        for (const auto &[name, index] : native_names)
        {
            auto symbol_type = native_registry->get_native_object_type(index);

            auto symbol = std::make_shared<Symbol>(
                name,
                symbol_type,
                false, // is_public
                false, // is_mutable
                true,  // is_native
                false, // is_captured
                0,     // closure_depth
                0      // lexical_depth
            );

            current_scope->define(symbol);
        }
    }

    // ============================================================================
    // SCOPE MANAGEMENT
    // ============================================================================

    void SemanticAnalyzer::enter_scope(ScopeType scope_type)
    {
        current_scope = std::make_shared<SymbolScope>(scope_type, current_scope);
    }

    void SemanticAnalyzer::leave_scope()
    {
        if (current_scope)
        {
            current_scope = current_scope->get_enclosing();
        }
    }

    // ============================================================================
    // High Level Visitors
    // ============================================================================

    void SemanticAnalyzer::visit(std::vector<Statement_ptr> &statements)
    {
        for (const auto &stmt : statements)
        {
            visit(stmt);
        }
    }

    void SemanticAnalyzer::visit(const Statement_ptr statement)
    {
        NULL_CHECK(statement);

        std::visit(overloaded{[&](ExpressionStatement &stat)
                              { visit(stat); },
                              [&](VariableDefinition &stat)
                              { visit(stat); },
                              [&](AliasDefinition &stat)
                              { visit(stat); },
                              [&](EnumDefinition &stat)
                              { visit(stat); },
                              [&](FunctionDefinition &stat)
                              { visit(stat); },
                              [&](ClassDefinition &stat)
                              { visit(stat); },
                              [&](TraitDefinition &stat)
                              { visit(stat); },
                              [&](ImplDefinition &stat)
                              { visit(stat); },
                              [&](AnnotationDefinition &stat)
                              { visit(stat); },
                              [&](IfBranch &stat)
                              { visit(stat); },
                              [&](ElseBranch &stat)
                              { visit(stat); },
                              [&](SimpleLoop &stat)
                              { visit(stat); },
                              [&](ForInLoop &stat)
                              { visit(stat); },
                              [&](LoopControl &stat)
                              { visit(stat); },
                              [&](Pass &stat)
                              { visit(stat); },
                              [&](Return &stat)
                              { visit(stat); },
                              [](auto)
                              { FATAL("Unhandled Statement type in Semantic Analyzer!"); }},
                   statement->data);
    }

    void SemanticAnalyzer::visit(ExpressionStatement &statement)
    {
        visit(statement.expression);
    }

    ObjectVector SemanticAnalyzer::visit(ExpressionVector expressions)
    {
        ObjectVector computed_types;
        computed_types.reserve(expressions.size());

        for (const auto &expr : expressions)
        {
            computed_types.push_back(visit(expr));
        }

        return computed_types;
    }

    Object_ptr SemanticAnalyzer::visit(const Expression_ptr expr)
    {
        NULL_CHECK(expr);

        return std::visit(overloaded{// Primitives
                                     [&](int &node) -> Object_ptr
                                     { return visit(node); },
                                     [&](double &node) -> Object_ptr
                                     { return visit(node); },
                                     [&](std::string &node) -> Object_ptr
                                     { return visit(node); },
                                     [&](bool &node) -> Object_ptr
                                     { return visit(node); },

                                     // Identifiers & Access
                                     [&](Identifier &node) -> Object_ptr
                                     { return visit(node); },
                                     [&](DotLiteral &node) -> Object_ptr
                                     { return visit(node); },

                                     // Operators
                                     [&](Prefix &node) -> Object_ptr
                                     { return visit(node); },
                                     [&](Infix &node) -> Object_ptr
                                     { return visit(node); },
                                     [&](Postfix &node) -> Object_ptr
                                     { return visit(node); },

                                     // Collections
                                     [&](ListLiteral &node) -> Object_ptr
                                     { return visit(node); },
                                     [&](TupleLiteral &node) -> Object_ptr
                                     { return visit(node); },
                                     [&](MapLiteral &node) -> Object_ptr
                                     { return visit(node); },
                                     [&](SetLiteral &node) -> Object_ptr
                                     { return visit(node); },
                                     [&](RangeLiteral &node) -> Object_ptr
                                     { return visit(node); },

                                     // Variables & Assignments
                                     [&](VariableDefinitionExpression &node) -> Object_ptr
                                     { return visit(node); },
                                     [&](UntypedAssignment &node) -> Object_ptr
                                     { return visit(node); },
                                     [&](TypedAssignment &node) -> Object_ptr
                                     { return visit(node); },
                                     [&](TypePattern &node) -> Object_ptr
                                     { return visit(node); },

                                     // Control Flow
                                     [&](IfTernaryBranch &node) -> Object_ptr
                                     { return visit(node); },
                                     [&](ElseTernaryBranch &node) -> Object_ptr
                                     { return visit(node); },
                                     [&](Call &node) -> Object_ptr
                                     { return visit(node); },

                                     // Fallback
                                     [](auto &) -> Object_ptr
                                     {
                                         FATAL("Semantic Error: Unhandled Expression node in visitor.");
                                         return nullptr;
                                     }},
                          expr->data);
    }

    // ============================================================================
    // Branching
    // ============================================================================

    Object_ptr SemanticAnalyzer::visit(IfTernaryBranch &expr)
    {
        enter_scope(ScopeType::BRANCH);

        Object_ptr cond_type = visit(expr.test);
        type_system->expect_condition_type(current_scope, cond_type);

        Object_ptr then_type = visit(expr.true_expression);
        leave_scope();

        if (expr.alternative)
        {
            Object_ptr else_type = visit(expr.alternative);
            ObjectVector unique_types = type_system->remove_duplicates(current_scope, {then_type, else_type});

            if (unique_types.size() == 1)
            {
                return unique_types[0];
            }

            return MAKE_OBJECT_VARIANT(VariantType(unique_types));
        }

        Object_ptr none_type = MAKE_OBJECT_VARIANT(NoneType());
        ObjectVector unique_types = type_system->remove_duplicates(current_scope, {then_type, none_type});

        if (unique_types.size() == 1)
            return unique_types[0];

        return MAKE_OBJECT_VARIANT(VariantType(unique_types));
    }

    void SemanticAnalyzer::visit(IfBranch &statement)
    {
        enter_scope(ScopeType::BRANCH);

        Object_ptr cond_type = visit(statement.test);
        type_system->expect_condition_type(current_scope, cond_type);

        visit(statement.body);
        leave_scope();

        if (statement.alternative)
        {
            visit(*statement.alternative);
        }
    }

    Object_ptr SemanticAnalyzer::visit(ElseTernaryBranch &expr)
    {
        enter_scope(ScopeType::BRANCH);
        Object_ptr type = visit(expr.expression);
        leave_scope();

        return type;
    }

    void SemanticAnalyzer::visit(ElseBranch &statement)
    {
        enter_scope(ScopeType::BRANCH);
        visit(statement.body);
        leave_scope();
    }

    void SemanticAnalyzer::visit(Pass &statement) {}

    // ---------------------------------------------------------------------------
    // Loops
    // ---------------------------------------------------------------------------

    void SemanticAnalyzer::visit(SimpleLoop &statement)
    {
        visit(statement.condition);
        enter_scope(ScopeType::LOOP);
        visit(statement.body);
        leave_scope();
    }

    void SemanticAnalyzer::visit(ForInLoop &statement)
    {
        visit(statement.iterable_expression);
        enter_scope(ScopeType::LOOP);

        if (statement.lhs->is<Identifier>())
        {
            auto &identifier_ast_node = statement.lhs->as<Identifier>();
            std::string name = identifier_ast_node.name;

            auto symbol = std::make_shared<Symbol>(
                name,
                nullptr,
                false,                              // is_public
                statement.lhs_is_mutable,           // is_mutable
                false,                              // is_native
                false,                              // is_captured
                current_scope->get_closure_depth(), // closure_depth
                current_scope->get_lexical_depth()  // lexical_depth
            );

            current_scope->define(symbol);
            identifier_ast_node.symbol = symbol;
        }
        else
        {
            FATAL("Semantic Error: For-in loop variable must be an identifier.");
        }

        visit(statement.body);
        leave_scope();
    }

    void SemanticAnalyzer::visit(LoopControl &statement)
    {
        SymbolScope_ptr scope = current_scope;

        if (!scope->enclosed_in(ScopeType::LOOP))
        {
            FATAL("Semantic Error: Loop control statement ('break', 'continue', 'redo') must be inside a loop.");
        }
    }

    // --------------------------------------------------------------------------
    // Definitions
    // --------------------------------------------------------------------------

    void SemanticAnalyzer::visit(AliasDefinition &statement) {}

    void SemanticAnalyzer::visit(EnumDefinition &statement) {}

    void SemanticAnalyzer::visit(ClassDefinition &statement) {}

    void SemanticAnalyzer::visit(TraitDefinition &statement) {}

    void SemanticAnalyzer::visit(ImplDefinition &statement) {}

    void SemanticAnalyzer::visit(AnnotationDefinition &statement) {}

    // ---------------------------------------------------------------------------
    // Variable Definitions & Assignments
    // --------------------------------------------------------------------------

    void SemanticAnalyzer::visit(VariableDefinition &statement)
    {
        define_variable(statement.expression, statement.is_mutable);
    }

    Object_ptr SemanticAnalyzer::visit(VariableDefinitionExpression &expr)
    {
        return define_variable(expr.assignment, expr.is_mutable);
    }

    Object_ptr SemanticAnalyzer::visit(UntypedAssignment &expr)
    {
        return mutate_variable(expr.lhs_expression, expr.rhs_expression);
    }

    Object_ptr SemanticAnalyzer::visit(TypedAssignment &expr)
    {
        FATAL("Internal Semantic Error: TypedAssignment visited directly.");
        return nullptr;
    }

    Object_ptr SemanticAnalyzer::define_variable(Expression_ptr assignment_expr, bool is_mutable)
    {
        Expression_ptr lhs_expr = nullptr;
        Expression_ptr rhs_expr = nullptr;
        Object_ptr expected_type = nullptr;

        if (assignment_expr->is<UntypedAssignment>())
        {
            auto &assign = assignment_expr->as<UntypedAssignment>();
            lhs_expr = assign.lhs_expression;
            rhs_expr = assign.rhs_expression;
        }
        else if (assignment_expr->is<TypedAssignment>())
        {
            auto &assign = assignment_expr->as<TypedAssignment>();
            lhs_expr = assign.lhs_expression;
            rhs_expr = assign.rhs_expression;
            expected_type = visit(assign.type_node);
        }
        else
        {
            FATAL("Semantic Error: Invalid variable definition expression.");
        }

        if (!lhs_expr->is<Identifier>())
        {
            FATAL("Semantic Error: Left-hand side of definition must be an Identifier.");
        }

        std::string var_name = lhs_expr->as<Identifier>().name;

        if (current_scope->contains_in_current_scope(var_name))
        {
            FATAL("Semantic Error: Variable '" + var_name + "' is already defined in this scope.");
        }

        // Resolve RHS Type
        Object_ptr actual_type = visit(rhs_expr);
        Object_ptr final_type = actual_type;

        if (expected_type)
        {
            if (!type_system->assignable(current_scope, expected_type, actual_type))
            {
                FATAL("Semantic Error: Type mismatch in variable definition for '" + var_name + "'.");
            }

            final_type = expected_type;
        }

        auto symbol = std::make_shared<Symbol>(
            var_name,
            final_type,
            false, // is_public
            is_mutable,
            false, // is_native
            false, // is_captured
            current_scope->get_closure_depth(),
            current_scope->get_lexical_depth()
        );

        current_scope->define(symbol);
        lhs_expr->as<Identifier>().symbol = symbol;

        return final_type;
    }

    Object_ptr SemanticAnalyzer::mutate_variable(Expression_ptr lhs_expr, Expression_ptr rhs_expr)
    {
        if (!lhs_expr->is<Identifier>())
        {
            FATAL("Semantic Error: Left-hand side of assignment must be an Identifier.");
        }

        auto &identifier_ast_node = lhs_expr->as<Identifier>();

        std::string var_name = identifier_ast_node.name;
        auto symbol = current_scope->lookup(var_name);

        if (!symbol)
        {
            FATAL("Semantic Error: Cannot assign to undefined variable '" + var_name + "'.");
        }

        if (!symbol->is_mutable)
        {
            FATAL("Semantic Error: Cannot reassign immutable variable '" + var_name + "'.");
        }

        if (symbol->should_be_captured(current_scope->get_closure_depth()))
        {
            symbol->is_captured = true;
        }

        identifier_ast_node.symbol = symbol;

        // Type Checking
        Object_ptr rhs_type = visit(rhs_expr);

        if (!type_system->assignable(current_scope, symbol->type, rhs_type))
        {
            FATAL("Semantic Error: Type mismatch in assignment to '" + var_name + "'.");
        }

        return rhs_type;
    }

    // ============================================================================
    // Functions & Calls
    // ============================================================================

    void SemanticAnalyzer::visit(FunctionDefinition &statement)
    {
        Object_ptr return_type = MAKE_OBJECT_VARIANT(NoneType());

        if (statement.return_type)
        {
            return_type = visit(statement.return_type);
        }

        ObjectVector param_types;
        std::vector<std::string> param_names;

        for (const auto &[name, type_ann] : statement.parameters)
        {
            Object_ptr p_type = MAKE_OBJECT_VARIANT(AnyType());

            if (type_ann)
            {
                p_type = visit(type_ann);
            }

            param_names.push_back(name);
            param_types.push_back(p_type);
        }

        auto func_type = MAKE_OBJECT_VARIANT(FunctionType(param_types, return_type));

        auto func_symbol = std::make_shared<Symbol>(
            statement.name,
            func_type,
            false, // is_public
            false, // is_mutable
            false, // is_native
            false, // is_captured
            current_scope->get_closure_depth(),
            current_scope->get_lexical_depth()
        );

        current_scope->define(func_symbol);
        statement.symbol = func_symbol;

        enter_scope(ScopeType::FUNCTION);
        return_type_stack.push_back(return_type);

        // Define Parameters as Local Variables in the New Scope
        for (size_t i = 0; i < param_names.size(); ++i)
        {
            current_scope->define(
                std::make_shared<Symbol>(
                    param_names[i],
                    param_types[i],
                    false, // is_public
                    false, // is_mutable
                    false, // is_native
                    false, // is_captured
                    current_scope->get_closure_depth(),
                    current_scope->get_lexical_depth()
                )
            );
        }

        visit(statement.body);

        return_type_stack.pop_back();
        leave_scope();
    }

    void SemanticAnalyzer::visit(Return &statement)
    {
        if (return_type_stack.empty())
        {
            FATAL("Semantic Error: 'return' statement used outside of a function.");
        }

        Object_ptr expected_type = return_type_stack.back();

        Object_ptr actual_type = MAKE_OBJECT_VARIANT(NoneType());

        if (statement.expression.has_value())
        {
            actual_type = visit(statement.expression.value());
        }

        if (!type_system->assignable(current_scope, expected_type, actual_type))
        {
            FATAL("Semantic Error: Return type mismatch. Expected " +
                  Wasp::stringify_object(expected_type) + ", got " +
                  Wasp::stringify_object(actual_type));
        }
    }

    Object_ptr SemanticAnalyzer::visit(Call &expr)
    {
        Object_ptr callee_type = visit(expr.callee);

        if (!callee_type->is<FunctionType>())
        {
            FATAL("Semantic Error: Expression is not callable. Type is: " + Wasp::stringify_object(callee_type));
        }

        if (expr.callee->is<Identifier>())
        {
            expr.symbol = expr.callee->as<Identifier>().symbol;
        }

        const auto &func = callee_type->as<FunctionType>();

        //  Check Argument Count
        if (expr.arguments.size() != func.input_types.size())
        {
            FATAL("Semantic Error: Incorrect number of arguments. Expected " +
                  std::to_string(func.input_types.size()) + ", got " +
                  std::to_string(expr.arguments.size()));
        }

        // Check Argument Types
        for (size_t i = 0; i < expr.arguments.size(); ++i)
        {
            Object_ptr arg_type = visit(expr.arguments[i]);
            Object_ptr param_type = func.input_types[i];

            if (!type_system->assignable(current_scope, param_type, arg_type))
            {
                FATAL("Semantic Error: Argument mismatch at index " + std::to_string(i) +
                      ". Expected " + Wasp::stringify_object(param_type) +
                      ", got " + Wasp::stringify_object(arg_type));
            }
        }

        if (func.return_type.has_value())
        {
            return func.return_type.value();
        }

        return MAKE_OBJECT_VARIANT(NoneType());
    }

    // ============================================================================
    // EXPRESSION
    // ============================================================================

    Object_ptr SemanticAnalyzer::visit(int expr) { return MAKE_OBJECT_VARIANT(IntType()); }

    Object_ptr SemanticAnalyzer::visit(double expr) { return MAKE_OBJECT_VARIANT(FloatType()); }

    Object_ptr SemanticAnalyzer::visit(std::string expr) { return MAKE_OBJECT_VARIANT(StringType()); }

    Object_ptr SemanticAnalyzer::visit(bool expr) { return MAKE_OBJECT_VARIANT(BooleanType()); }

    // --- Access ---

    Object_ptr SemanticAnalyzer::visit(Identifier &expr)
    {
        auto symbol = current_scope->lookup(expr.name);

        if (!symbol)
        {
            FATAL("Semantic Error: Undefined variable '" + expr.name + "'");
        }

        if (symbol->is_native) {
            // type checking later
        } else if (!symbol->is_captured &&
                   symbol->should_be_captured(current_scope->get_closure_depth())) {
            symbol->is_captured = true;
        }

        expr.symbol = symbol;

        return symbol->type;
    }

    Object_ptr SemanticAnalyzer::visit(DotLiteral &expr) { return nullptr; }

    // --- Operators ---

    Object_ptr SemanticAnalyzer::visit(Prefix &expr)
    {
        Object_ptr right_type = visit(expr.operand);
        return type_system->infer(current_scope, right_type, expr.op.type);
    }

    Object_ptr SemanticAnalyzer::visit(Infix &expr)
    {
        Object_ptr left_type = visit(expr.left);
        Object_ptr right_type = visit(expr.right);
        return type_system->infer(current_scope, left_type, expr.op.type, right_type);
    }

    Object_ptr SemanticAnalyzer::visit(Postfix &expr)
    {
        Object_ptr left_type = visit(expr.operand);
        type_system->expect_number_type(left_type);
        return left_type;
    }

    Object_ptr SemanticAnalyzer::visit(ListLiteral &expr)
    {
        if (expr.expressions.empty())
            return MAKE_OBJECT_VARIANT(ListType(MAKE_OBJECT_VARIANT(AnyType())));

        ObjectVector element_types;
        for (const auto &element : expr.expressions)
            element_types.push_back(visit(element));

        ObjectVector unique_types = type_system->remove_duplicates(current_scope, element_types);

        if (unique_types.size() == 1)
            return MAKE_OBJECT_VARIANT(ListType(unique_types[0]));
        return MAKE_OBJECT_VARIANT(ListType(MAKE_OBJECT_VARIANT(VariantType(unique_types))));
    }

    Object_ptr SemanticAnalyzer::visit(TupleLiteral &expr)
    {
        ObjectVector element_types;
        for (const auto &element : expr.expressions)
            element_types.push_back(visit(element));
        return MAKE_OBJECT_VARIANT(TupleType(element_types));
    }

    Object_ptr SemanticAnalyzer::visit(MapLiteral &expr)
    {
        if (expr.pairs.empty())
            return MAKE_OBJECT_VARIANT(MapType(MAKE_OBJECT_VARIANT(AnyType()), MAKE_OBJECT_VARIANT(AnyType())));

        ObjectVector key_types;
        ObjectVector val_types;

        for (const auto &[key_expr, val_expr] : expr.pairs)
        {
            Object_ptr k_type = visit(key_expr);
            type_system->expect_key_type(current_scope, k_type);
            key_types.push_back(k_type);
            val_types.push_back(visit(val_expr));
        }

        ObjectVector unique_keys = type_system->remove_duplicates(current_scope, key_types);
        ObjectVector unique_vals = type_system->remove_duplicates(current_scope, val_types);

        Object_ptr final_key_type = unique_keys.size() == 1 ? unique_keys[0] : MAKE_OBJECT_VARIANT(VariantType(unique_keys));
        Object_ptr final_val_type = unique_vals.size() == 1 ? unique_vals[0] : MAKE_OBJECT_VARIANT(VariantType(unique_vals));

        return MAKE_OBJECT_VARIANT(MapType(final_key_type, final_val_type));
    }

    Object_ptr SemanticAnalyzer::visit(SetLiteral &expr)
    {
        if (expr.expressions.empty())
            return MAKE_OBJECT_VARIANT(SetType(MAKE_OBJECT_VARIANT(AnyType())));

        ObjectVector element_types;
        for (const auto &element : expr.expressions)
        {
            Object_ptr el_type = visit(element);
            type_system->expect_key_type(current_scope, el_type);
            element_types.push_back(el_type);
        }

        ObjectVector unique_types = type_system->remove_duplicates(current_scope, element_types);

        if (unique_types.size() == 1)
            return MAKE_OBJECT_VARIANT(SetType(unique_types[0]));
        return MAKE_OBJECT_VARIANT(SetType(MAKE_OBJECT_VARIANT(VariantType(unique_types))));
    }

    Object_ptr SemanticAnalyzer::visit(RangeLiteral &expr)
    {
        Object_ptr start_type = nullptr;
        Object_ptr end_type = nullptr;

        if (expr.start)
        {
            start_type = visit(expr.start);
            type_system->expect_number_type(start_type);
        }

        if (expr.end)
        {
            end_type = visit(expr.end);
            type_system->expect_number_type(end_type);
        }

        if (type_system->is_float_type(start_type) || type_system->is_float_type(end_type))
        {
            return MAKE_OBJECT_VARIANT(ListType(MAKE_OBJECT_VARIANT(FloatType())));
        }

        return MAKE_OBJECT_VARIANT(ListType(MAKE_OBJECT_VARIANT(IntType())));
    }

    Object_ptr SemanticAnalyzer::visit(TypePattern &expr) { return nullptr; }
}
