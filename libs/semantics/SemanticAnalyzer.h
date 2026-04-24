#pragma once

#include "AST.h"
#include "Expression.h"
#include "Objects.h"
#include "Statement.h"
#include "SymbolScope.h"
#include "TypeAnnotation.h"
#include "TypeChecker.h"
#include "Workspace.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace Wasp
{

class SemanticAnalyzer
{
    Workspace_ptr workspace;
    TypeChecker_ptr type_checker;

    SymbolScope_ptr current_scope;
    ObjectVector return_type_stack;

    // -------------------------------------------------------------------------
    // Statement Visitors
    // -------------------------------------------------------------------------

    void setup_exports(Module_ptr mod, StringVector ordered_export_names);

    void visit(const Statement_ptr statement);
    void visit(StatementVector& statements);

    void visit(ExpressionStatement& statement);

    void hoist_statements(StatementVector& statements);
    StringVector setup_ordered_export_names(Module_ptr mod);

    std::pair<Object_ptr, ObjectVector> get_function_signature(AbstractFunctionDefinition& func);
    std::pair<Object_ptr, ObjectVector> get_function_signature(Object_ptr type_obj);

    template <typename T> void analyze_function_base(T& def, ScopeType scope_type, bool is_mutable);
    template <typename T> void analyze_instance_method(Object_ptr class_type_obj, T& m);
    template <typename T> void analyze_pure_method(T& m);
    template <typename T> void hoist_method(std::shared_ptr<ClassType>& class_type, T& m);

    void visit(FunctionDefinition& statement);
    void visit(PureFunctionDefinition& statement);

    ClassType_ptr initialize_class_type(ClassDefinition& def);

    void visit(ClassDefinition& statement);
    void visit(FieldDefinition& statement);

    void visit(AliasDefinition& statement);
    void visit(EnumDefinition& statement);
    void visit(AnnotationDefinition& statement);

    void visit(IfBranch& statement);
    void visit(ElseBranch& statement);
    void visit(SimpleLoop& statement);
    void visit(ForInLoop& statement);
    void visit(LoopControl& statement);

    void visit(Pass& statement);
    void visit(Return& statement);

    // ------------------------------------------------------------------------
    // Imports Visitors
    // ------------------------------------------------------------------------

    void visit(SimpleImport& statement);
    void visit(FromImport& statement);

    // ------------------------------------------------------------------------
    // Variables & Assignments
    // ------------------------------------------------------------------------

    Object_ptr define_variable(Expression_ptr assignment_expr, bool is_mutable);
    Object_ptr mutate_variable(Expression_ptr lhs_expr, Expression_ptr rhs_expr);
    Object_ptr mutate_member(Expression_ptr lhs_expr, Expression_ptr rhs_expr);
    void validate_purity_constraints(Symbol_ptr target_symbol) const;
    void visit(VariableDefinition& statement);

    Object_ptr visit(VariableDefinitionExpression& expr);
    Object_ptr visit(UntypedAssignment& expr);
    Object_ptr visit(TypedAssignment& expr);

    // ========================================================================
    // Expression Visitors
    // ========================================================================

    Object_ptr visit(const Expression_ptr expr);
    ObjectVector visit(ExpressionVector expressions);

    Object_ptr visit(int expr);
    Object_ptr visit(double expr);
    Object_ptr visit(std::string expr);
    Object_ptr visit(bool expr);

    Object_ptr visit(DotLiteral& expr);

    Object_ptr visit(Identifier& expr);
    Object_ptr visit(MemberAccess& expr);

    Object_ptr get_function_return_type(Symbol_ptr symbol);
    bool is_native_function(Symbol_ptr symbol);

    Object_ptr evaluate_function_call(
        Call& call_expr,
        Identifier& callable_identifier,
        const ObjectVector& arg_types,
        Symbol_ptr function_overload_symbol
    );

    Object_ptr evaluate_module_function_call(
        Call& call_expr,
        MemberAccess& mac,
        const ObjectVector& arg_types
    );

    Object_ptr evaluate_instance_creation(
        Constructor& constructor,
        Identifier& callable_identifier,
        Symbol_ptr symbol,
        const ObjectVector& arg_types
    );

    Object_ptr evaluate_module_instance_creation(
        Constructor& constructor,
        MemberAccess& access,
        const ObjectVector& arg_types
    );

    Object_ptr evaluate_instance_method_call(
        Call& call_expr,
        MemberAccess& mac,
        const ObjectVector& arg_types,
        ClassType_ptr class_type
    );

    Object_ptr visit(Call& expr);
    Object_ptr visit(Constructor& expr);

    Object_ptr visit(Prefix& expr);
    Object_ptr visit(Infix& expr);
    Object_ptr visit(Postfix& expr);

    Object_ptr visit(ListLiteral& expr);
    Object_ptr visit(TupleLiteral& expr);
    Object_ptr visit(MapLiteral& expr);
    Object_ptr visit(SetLiteral& expr);
    Object_ptr visit(RangeLiteral& expr);

    Object_ptr visit(TypePattern& expr);

    Object_ptr visit(IfTernaryBranch& expr);
    Object_ptr visit(ElseTernaryBranch& expr);

    // ========================================================================
    // Type Visitors
    // ========================================================================

    Object_ptr visit(const TypeAnnotation_ptr type_node);
    ObjectVector visit(std::vector<TypeAnnotation_ptr>& type_nodes);

    // Primitive Types
    Object_ptr visit(AnyTypeNode& expr);
    Object_ptr visit(NoneTypeNode& expr);

    Object_ptr visit(IntTypeNode& expr);
    Object_ptr visit(FloatTypeNode& expr);
    Object_ptr visit(StringTypeNode& expr);
    Object_ptr visit(BoolTypeNode& expr);

    Object_ptr visit(IntLiteralTypeNode& expr);
    Object_ptr visit(FloatLiteralTypeNode& expr);
    Object_ptr visit(StringLiteralTypeNode& expr);
    Object_ptr visit(BoolLiteralTypeNode& expr);

    Object_ptr visit(TypeIdentifierNode& expr);

    Object_ptr visit(ListTypeNode& expr);
    Object_ptr visit(TupleTypeNode& expr);
    Object_ptr visit(SetTypeNode& expr);
    Object_ptr visit(MapTypeNode& expr);
    Object_ptr visit(VariantTypeNode& expr);
    Object_ptr visit(FunctionTypeNode& expr);
    Object_ptr visit(RecordTypeNode& expr);

    // ========================================================================
    // Utils
    // ========================================================================

    void enter_scope(ScopeType scope_type);
    void leave_scope();
    void leave_scope_keep_symbol(Symbol_ptr symbol_to_keep);

    void register_natives();
    void extract_module_type(Module_ptr module);

public:
    SemanticAnalyzer(Workspace_ptr workspace)
        : type_checker(std::make_shared<TypeChecker>(workspace->pool)), workspace(workspace) {};

    void run(const std::vector<Module_ptr>& build_order);
};

} // namespace Wasp
