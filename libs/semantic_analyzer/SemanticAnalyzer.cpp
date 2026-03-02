#include "SemanticAnalyzer.h"
#include <stdexcept>

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
    void SemanticAnalyzer::run(struct Module &ast)
    {
        enter_scope(ScopeType::MODULE);
        visit(ast.statements);
        leave_scope();
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
    // LITERAL TYPES
    // ============================================================================

    Object_ptr SemanticAnalyzer::visit(int expr)
    {
        return MAKE_OBJECT_VARIANT(IntLiteralType(expr));
    }

    Object_ptr SemanticAnalyzer::visit(double expr)
    {
        return MAKE_OBJECT_VARIANT(FloatLiteralType(expr));
    }

    Object_ptr SemanticAnalyzer::visit(std::string expr)
    {
        return MAKE_OBJECT_VARIANT(StringLiteralType(expr));
    }

    Object_ptr SemanticAnalyzer::visit(bool expr)
    {
        return MAKE_OBJECT_VARIANT(BooleanLiteralType(expr));
    }

    // ============================================================================
    // IDENTIFIERS (Variable Lookup)
    // ============================================================================

    Object_ptr SemanticAnalyzer::visit(Identifier &expr)
    {
        auto symbol = current_scope->lookup(expr.name);

        if (!symbol)
        {
            FATAL("Semantic Error: Undefined variable '" + expr.name + "'");
        }

        return symbol->type;
    }

    // ============================================================================
    // STATEMENT VISITORS
    // ============================================================================

    void SemanticAnalyzer::visit(const Statement_ptr statement)
    {
        NULL_CHECK(statement);

        std::visit(overloaded{[&](ExpressionStatement &stat)
                              { visit(stat); },
                              [&](VariableDefinition &var_def)
                              { visit(var_def); },

                              [&](AliasDefinition &alias_def)
                              { visit(alias_def); },
                              [&](EnumDefinition &enum_def)
                              { visit(enum_def); },
                              [&](FunctionDefinition &func_def)
                              { visit(func_def); },
                              [&](ClassDefinition &class_def)
                              { visit(class_def); },
                              [&](TraitDefinition &trait_def)
                              { visit(trait_def); },
                              [&](ImplDefinition &impl_def)
                              { visit(impl_def); },
                              [&](AnnotationDefinition &anno_def)
                              { visit(anno_def); },

                              [&](IfBranch &if_branch)
                              { visit(if_branch); },
                              [&](ElseBranch &else_branch)
                              { visit(else_branch); },

                              [&](SimpleLoop &simple_loop)
                              { visit(simple_loop); },
                              [&](ForInLoop &for_in_loop)
                              { visit(for_in_loop); },
                              [&](LoopControl &loop_control)
                              { visit(loop_control); },

                              [&](Pass &pass_stmt)
                              { visit(pass_stmt); },
                              [&](Return &return_stmt)
                              { visit(return_stmt); },

                              [](auto)
                              { FATAL("Unhandled Statement type in Semantic Analyzer!"); }},
                   statement->data);
    }

    void SemanticAnalyzer::visit(std::vector<Statement_ptr> &statements)
    {
        for (const auto &stmt : statements)
        {
            visit(stmt);
        }
    }

    void SemanticAnalyzer::visit(ExpressionStatement &statement)
    {
        visit(statement.expression);
    }

    void SemanticAnalyzer::visit(VariableDefinition &statement)
    {
        Expression_ptr lhs_expr = nullptr;
        Expression_ptr rhs_expr = nullptr;
        Object_ptr expected_type = nullptr;

        if (statement.expression->is<UntypedAssignment>())
        {
            auto &assign = statement.expression->as<UntypedAssignment>();
            lhs_expr = assign.lhs_expression;
            rhs_expr = assign.rhs_expression;
        }
        else if (statement.expression->is<TypedAssignment>())
        {
            auto &assign = statement.expression->as<TypedAssignment>();
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

        auto existing_symbol = current_scope->lookup(var_name);
        if (existing_symbol && existing_symbol->depth == current_scope->get_depth())
        {
            FATAL("Semantic Error: Variable '" + var_name + "' is already defined in this scope.");
        }

        Object_ptr actual_type = visit(rhs_expr);
        Object_ptr final_type = actual_type;

        // Perform type checking if an explicit type annotation was provided
        if (expected_type)
        {
            if (!type_system->assignable(current_scope, expected_type, actual_type))
            {
                FATAL("Semantic Error: Type mismatch in variable definition for '" + var_name + "'.");
            }

            final_type = expected_type;
        }

        int symbol_id = next_id++;
        auto symbol = std::make_shared<Symbol>(
            symbol_id,
            var_name,
            final_type,
            false,
            statement.is_mutable,
            current_scope->get_depth());

        current_scope->define(var_name, symbol);
    }

    // ============================================================================
    // UTILITIES
    // ============================================================================

    std::tuple<std::string, Object_ptr> SemanticAnalyzer::deconstruct_type_pattern(Expression_ptr expression)
    {
        // Used for advanced pattern matching or typed assignments (e.g., `let x: int = 5`).
        // Will extract the variable name and its expected TypeObject.
        return {"", nullptr};
    }

    bool SemanticAnalyzer::any_eq(ObjectVector vec, Object_ptr x)
    {
        // Helper to check if a specific TypeObject exists inside a vector of TypeObjects.
        // Crucial for Union/Variant type checking.
        return false;
    }

    ObjectVector SemanticAnalyzer::remove_duplicates(ObjectVector vec)
    {
        // Helper to simplify Variant types. (e.g., `int | int | string` becomes `int | string`).
        return {};
    }

    // ============================================================================
    // EXPRESSION VISITORS
    // ============================================================================

    Object_ptr SemanticAnalyzer::visit(const Expression_ptr expr)
    {
        // Standard variant dispatcher. Calls the specific visit() method based on the expression type.
        return nullptr;
    }

    ObjectVector SemanticAnalyzer::visit(ExpressionVector expressions)
    {
        // Loops over a vector of expressions, visits each, and returns a vector of their computed Types.
        return {};
    }

    Object_ptr SemanticAnalyzer::visit(Prefix &expr)
    {
        // 1. Visit the operand to get its type.
        // 2. Check if the operator is valid for that type (e.g., `!bool` is ok, `-string` is FATAL).
        // 3. Return the resulting type.
        return nullptr;
    }

    Object_ptr SemanticAnalyzer::visit(Infix &expr)
    {
        // 1. Visit the left and right operands to get their types.
        // 2. Check if the operator is valid for those types (e.g., `int + float` might promote to float, `int + string` is FATAL).
        // 3. Return the resulting type (e.g., a boolean type for comparison operators).
        return nullptr;
    }

    Object_ptr SemanticAnalyzer::visit(Postfix &expr)
    {
        // Similar to Prefix, validate the operand type and return the resulting type.
        return nullptr;
    }

    Object_ptr SemanticAnalyzer::visit(ListLiteral &expr)
    {
        // 1. Visit all elements to get their types.
        // 2. If all types are identical, return a ListType(ElementType).
        // 3. If types differ, return a ListType(VariantType(Types...)).
        return nullptr;
    }

    Object_ptr SemanticAnalyzer::visit(TupleLiteral &expr)
    {
        // 1. Visit all elements.
        // 2. Return a TupleType containing the exact sequence of element types.
        return nullptr;
    }

    Object_ptr SemanticAnalyzer::visit(UntypedAssignment &expr)
    {
        // 1. Ensure LHS is valid (e.g., an Identifier or property access).
        // 2. Look up LHS in scope; throw FATAL if immutable (e.g., checking `is_mutable` on the symbol).
        // 3. Evaluate RHS to get its type.
        // 4. Verify RHS type matches LHS existing type. Throw FATAL if mismatch.
        // 5. Return the RHS type.
        return nullptr;
    }

    Object_ptr SemanticAnalyzer::visit(TypedAssignment &expr)
    {
        // Similar to UntypedAssignment, but also cross-checks the RHS type against the explicit Type Annotation provided.
        return nullptr;
    }

    Object_ptr SemanticAnalyzer::visit(MapLiteral &expr) { return nullptr; }
    Object_ptr SemanticAnalyzer::visit(SetLiteral &expr) { return nullptr; }
    Object_ptr SemanticAnalyzer::visit(RangeLiteral &expr) { return nullptr; }
    Object_ptr SemanticAnalyzer::visit(TypePattern &expr) { return nullptr; }
    Object_ptr SemanticAnalyzer::visit(VariableDefinitionExpression &expr) { return nullptr; }
    Object_ptr SemanticAnalyzer::visit(IfTernaryBranch &expr) { return nullptr; }
    Object_ptr SemanticAnalyzer::visit(ElseTernaryBranch &expr) { return nullptr; }
    Object_ptr SemanticAnalyzer::visit(DotLiteral &expr) { return nullptr; }
    Object_ptr SemanticAnalyzer::infer_chain_member_type(Object_ptr lhs, Expression_ptr expr, bool null_check) { return nullptr; }

    Object_ptr SemanticAnalyzer::visit(Call &expr)
    {
        // 1. Visit callee to get its type. Verify it is a FunctionType.
        // 2. Visit all arguments to get their types.
        // 3. Compare argument types/count against the FunctionType's expected parameter types. Throw FATAL if mismatch.
        // 4. Return the FunctionType's declared return type.
        return nullptr;
    }

    // ============================================================================
    // STATEMENT VISITORS (These do NOT return types, they just enforce rules)
    // ============================================================================

    void SemanticAnalyzer::visit(IfBranch &statement)
    {
        // 1. Visit the condition. Throw FATAL if it does not evaluate to a BooleanType.
        // 2. enter_scope(ScopeType::BRANCH).
        // 3. Visit the body.
        // 4. leave_scope().
        // 5. If there is an alternative (elif/else), visit it too.
    }

    void SemanticAnalyzer::visit(Return &statement)
    {
        // 1. Ensure we are inside a FUNCTION scope. Throw FATAL if returning from module level.
        // 2. Evaluate the return expression to get its Type (or NoneType if empty).
        // 3. Look up the current function's expected return type.
        // 4. Throw FATAL if the expression Type doesn't match the expected return type.
    }

    // --- Statement Stubs ---

    void SemanticAnalyzer::visit(FunctionDefinition &statement)
    {
        // 1. Create a FunctionType object representing the signature.
        // 2. Define the function name in the CURRENT scope.
        // 3. enter_scope(ScopeType::FUNCTION).
        // 4. Add parameters as defined variables in the NEW scope.
        // 5. Visit the body.
        // 6. leave_scope().
    }

    void SemanticAnalyzer::visit(AliasDefinition &statement) {}
    void SemanticAnalyzer::visit(EnumDefinition &statement) {}
    void SemanticAnalyzer::visit(ClassDefinition &statement) {}
    void SemanticAnalyzer::visit(TraitDefinition &statement) {}
    void SemanticAnalyzer::visit(ImplDefinition &statement) {}
    void SemanticAnalyzer::visit(AnnotationDefinition &statement) {}
    void SemanticAnalyzer::visit(ElseBranch &statement) {}
    void SemanticAnalyzer::visit(SimpleLoop &statement) {}
    void SemanticAnalyzer::visit(ForInLoop &statement) {}
    void SemanticAnalyzer::visit(LoopControl &statement) {}
    void SemanticAnalyzer::visit(Pass &statement) {}

    // ============================================================================
    // TYPE VISITORS (Converting AST Type Annotations -> Runtime Type Objects)
    // ============================================================================

    Object_ptr SemanticAnalyzer::visit(const TypeAnnotation_ptr type_node)
    {
        // Variant dispatcher for AST Type Nodes.
        return nullptr;
    }

    ObjectVector SemanticAnalyzer::visit(std::vector<TypeAnnotation_ptr> &type_nodes)
    {
        return {};
    }

    // --- Primitive & Literal Types ---
    // These literally just return MAKE_OBJECT_VARIANT(Type());
    Object_ptr SemanticAnalyzer::visit(AnyTypeNode &expr) { return nullptr; }
    Object_ptr SemanticAnalyzer::visit(NoneTypeNode &expr) { return nullptr; }
    Object_ptr SemanticAnalyzer::visit(IntTypeNode &expr) { return nullptr; }
    Object_ptr SemanticAnalyzer::visit(FloatTypeNode &expr) { return nullptr; }
    Object_ptr SemanticAnalyzer::visit(StringTypeNode &expr) { return nullptr; }
    Object_ptr SemanticAnalyzer::visit(BoolTypeNode &expr) { return nullptr; }

    Object_ptr SemanticAnalyzer::visit(IntLiteralTypeNode &expr) { return nullptr; }
    Object_ptr SemanticAnalyzer::visit(FloatLiteralTypeNode &expr) { return nullptr; }
    Object_ptr SemanticAnalyzer::visit(StringLiteralTypeNode &expr) { return nullptr; }
    Object_ptr SemanticAnalyzer::visit(BoolLiteralTypeNode &expr) { return nullptr; }

    // --- Type Identifiers ---
    Object_ptr SemanticAnalyzer::visit(TypeIdentifierNode &expr)
    {
        // 1. Look up the custom type name (Class/Alias/Enum) in scope.
        // 2. Throw FATAL if not found or if it isn't actually a type.
        // 3. Return the resolved Type object.
        return nullptr;
    }

    // --- Composite Types ---
    // These visit their inner Type Annotations and wrap them in composite Type Objects.
    Object_ptr SemanticAnalyzer::visit(ListTypeNode &expr) { return nullptr; }
    Object_ptr SemanticAnalyzer::visit(TupleTypeNode &expr) { return nullptr; }
    Object_ptr SemanticAnalyzer::visit(SetTypeNode &expr) { return nullptr; }
    Object_ptr SemanticAnalyzer::visit(MapTypeNode &expr) { return nullptr; }
    Object_ptr SemanticAnalyzer::visit(VariantTypeNode &expr) { return nullptr; }
    Object_ptr SemanticAnalyzer::visit(FunctionTypeNode &expr) { return nullptr; }
    Object_ptr SemanticAnalyzer::visit(RecordTypeNode &expr) { return nullptr; }
}
