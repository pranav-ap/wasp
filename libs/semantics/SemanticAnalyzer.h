#pragma once

#include "AST.h"
#include "Expression.h"
#include "Objects.h"
#include "Statement.h"
#include "SymbolScope.h"
#include "TypeAnnotation.h"
#include "TypeSystem.h"
#include "Workspace.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace Wasp
{

class SemanticAnalyzer
{
public:
    SemanticAnalyzer(Workspace_ptr workspace)
        : type_checker(std::make_shared<TypeSystem>(workspace->pool)), workspace(workspace) {};

    void run(const std::vector<Module_ptr>& build_order);

private:
    Workspace_ptr workspace;
    TypeSystem_ptr type_checker;
    Module_ptr current_module = nullptr;
    SymbolScope_ptr current_scope;
    ObjectVector return_type_stack;

    // =========================================================================
    // Scope & Environment Management
    // =========================================================================
    void enter_scope(ScopeType scope_type);
    void leave_scope();
    void leave_scope_keep_symbol(Symbol_ptr symbol_to_keep);

    void extract_module_type(Module_ptr module);
    void setup_exports(Module_ptr mod, StringVector ordered_export_names);
    StringVector setup_ordered_export_names(Module_ptr mod);

    // =========================================================================
    // Statement Analysis
    // =========================================================================

    void visit(const Statement_ptr statement);
    void visit(StatementVector& statements);
    void hoist_statements(StatementVector& statements);

    void visit(ExpressionStatement& statement);
    void visit(FunctionDefinition& statement);
    void visit(MethodDefinition& statement);
    void visit(TemplateDefinition& statement);
    void visit(ClassDefinition& statement);
    void visit(TraitDefinition& statement);
    void visit(FieldDefinition& statement);
    void visit(EnumDefinition& statement);
    void visit(VariableDefinition& statement);
    void visit(TypeAliasDefinition& statement);
    void visit(AnnotationDefinition& statement);
    void visit(IfBranch& statement);
    void visit(ElseBranch& statement);
    void visit(SimpleLoop& statement);
    void visit(ForInLoop& statement);
    void visit(LoopControl& statement);
    void visit(SimpleImport& statement);
    void visit(FromImport& statement);
    void visit(Pass& statement);
    void visit(Native& statement);
    void visit(Return& statement);

    ClassType_ptr initialize_class_type(ClassDefinition& def);
    void analyze_membered_type(ClassDefinition& def, ClassType_ptr base_type);
    void analyze_function(
        FunctionDefinition& def,
        ScopeType scope_type,
        bool parameters_are_mutable
    );

    // =========================================================================
    // Expression Analysis
    // =========================================================================
    Object_ptr visit(const Expression_ptr expr);
    ObjectVector visit(ExpressionVector expressions);

    Object_ptr visit(int expr);
    Object_ptr visit(double expr);
    Object_ptr visit(std::string expr);
    Object_ptr visit(bool expr);
    Object_ptr visit(NoneLiteral& expr);
    Object_ptr visit(DotLiteral& expr);
    Object_ptr visit(ListLiteral& expr);
    Object_ptr visit(TupleLiteral& expr);
    Object_ptr visit(MapLiteral& expr);
    Object_ptr visit(SetLiteral& expr);
    Object_ptr visit(RangeLiteral& expr);
    Object_ptr visit(Prefix& expr);
    Object_ptr visit(Infix& expr);
    Object_ptr visit(Postfix& expr);
    Object_ptr visit(Identifier& expr);
    Object_ptr visit(MemberAccess& expr);
    Object_ptr visit(VariableDefinitionExpression& expr);
    Object_ptr visit(UntypedAssignment& expr);
    Object_ptr visit(TypedAssignment& expr);
    Object_ptr visit(TypePattern& expr);
    Object_ptr visit(IfTernaryBranch& expr);
    Object_ptr visit(ElseTernaryBranch& expr);
    Object_ptr visit(Call& expr);
    Object_ptr visit(Constructor& expr);
    Object_ptr visit(TemplateInstantiation& template_instantiation);

    Object_ptr define_variable(Expression_ptr assignment_expr, bool is_mutable);
    Object_ptr mutate_variable(Expression_ptr lhs_expr, Expression_ptr rhs_expr);
    Object_ptr mutate_member(Expression_ptr lhs_expr, Expression_ptr rhs_expr);

    // =========================================================================
    // Call & Instantiation Evaluators
    // =========================================================================
    std::pair<Object_ptr, ObjectVector> get_function_signature(AbstractFunctionDefinition& func);
    std::pair<Object_ptr, ObjectVector> get_function_signature(Object_ptr type_obj);
    Object_ptr get_function_return_type(Symbol_ptr symbol);
    bool is_native_function(Symbol_ptr symbol);
    void validate_purity_constraints(Symbol_ptr target_symbol) const;

    void bind_identifier(Identifier& id, Symbol_ptr symbol);
    Symbol_ptr resolve_module_export(MemberAccess& access);
    void validate_implicit_instance_creation(
        ClassType_ptr class_type,
        const ObjectVector& arg_types
    );

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
    Object_ptr evaluate_class_method_call(
        Call& call_expr,
        MemberAccess& mac,
        const ObjectVector& arg_types,
        ClassType_ptr class_type
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
    Object_ptr evaluate_template_call(
        Call& call_expr,
        TemplateInstantiation& template_instantiation,
        const ObjectVector& argument_types
    );
    Object_ptr evaluate_template_function_call(
        Call& call,
        TemplateInstantiation& template_instantiation,
        Identifier& target,
        const ObjectVector& argument_types,
        Symbol_ptr function_overload_symbol
    );
    Object_ptr evaluate_template_module_function_call(
        Call& call,
        TemplateInstantiation& template_instantiation,
        MemberAccess& access,
        const ObjectVector& argument_types
    );
    Object_ptr evaluate_template_method_call(
        Call& call,
        TemplateInstantiation& template_instantiation,
        MemberAccess& member_access,
        const ObjectVector& argument_types,
        ClassType_ptr class_type
    );
    Object_ptr evaluate_class_template_instantiation(
        Constructor& constructor,
        TemplateInstantiation& template_instantiation,
        Identifier& target,
        const ObjectVector& argument_types,
        const ObjectVector& generic_args,
        Symbol_ptr template_symbol
    );

    Object_ptr evaluate_type_template_instantiation(
        TemplateInstantiation& template_instantiation,
        Identifier& target,
        Symbol_ptr template_symbol,
        const ObjectVector& generic_args
    );

    // =========================================================================
    // Type Annotation Visitors
    // =========================================================================
    Object_ptr visit(const TypeAnnotation_ptr type_node);
    ObjectVector visit(TypeAnnotationVector& type_nodes);

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
    Object_ptr visit(TemplateTypeNode& node);
};
} // namespace Wasp
