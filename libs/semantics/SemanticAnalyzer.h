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
        : type_system(std::make_shared<TypeSystem>(workspace->pool)),
          workspace(workspace) {};

    void run(const std::vector<Module_ptr>& build_order);

private:
    Workspace_ptr workspace;
    TypeSystem_ptr type_system;
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

    void inject_prelude();
    void hoist_statements(StatementVector& statements);
    void hoist_names_and_imports(StatementVector& statements);
    void hoist_import(Import& stmt);
    void hoist_signatures_and_generics(StatementVector& statements);

    std::pair<ObjectStringMap, StringVector> evaluate_generics(
        const std::vector<FieldDefinition>& generic_fields
    );

    template <typename CallableDef> void hoist_callable(CallableDef& def);

    void visit(ExpressionStatement& statement);
    void visit(FunctionDefinition& statement);
    void visit(MethodDefinition& statement);
    void visit(OperatorDefinition& statement);
    void visit(ClassDefinition& statement);
    void visit(TraitDefinition& statement);
    void visit(FieldDefinition& statement);
    void visit(EnumDefinition& statement);
    void visit(TypeAliasDefinition& statement);
    void visit(IfBranch& statement);
    void visit(ElseBranch& statement);
    void visit(SimpleLoop& statement);
    void visit(ForInLoop& statement);
    void visit(LoopControl& statement);
    void visit(Import& statement);
    void visit(Pass& statement);
    void visit(Native& statement);
    void visit(Return& statement);

    bool prepare_generic_scope(const ObjectStringMap& generics);

    // =========================================================================
    // Expression Analysis
    // =========================================================================

    Object_ptr visit(const Expression_ptr expr);
    ObjectVector visit(ExpressionVector expressions);

    Object_ptr visit(IntegerLiteral& expr);
    Object_ptr visit(FloatLiteral& expr);
    Object_ptr visit(StringLiteral& expr);
    Object_ptr visit(BooleanLiteral& expr);
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
    Object_ptr visit(TemplateAngular& template_instantiation);

    Object_ptr visit(Assignment& expr);
    Object_ptr visit(IfTernaryBranch& expr);
    Object_ptr visit(ElseTernaryBranch& expr);

    Object_ptr visit(Call& expr);
    Object_ptr visit(Constructor& expr);

    Object_ptr define_variable(Assignment& assign);
    Object_ptr mutate_variable(
        Expression_ptr lhs_expr,
        Expression_ptr rhs_expr
    );
    Object_ptr mutate_member(Expression_ptr lhs_expr, Expression_ptr rhs_expr);

    Object_ptr collapse_types(const ObjectVector& types);
    Symbol_ptr get_core_symbol(const std::string& type_name);

    void desugar_literal(
        const Expression_ptr& expr,
        const std::string& type_alias_name
    );

    void desugar_overloaded_operator(
        const Expression_ptr& expr,
        Symbol_ptr operator_symbol,
        int overload_index,
        const std::vector<Expression_ptr>& arguments
    );

    // =========================================================================
    // Call Evaluators
    // =========================================================================

    bool is_native_function(Symbol_ptr symbol);
    void validate_purity_constraints(Symbol_ptr target_symbol) const;

    void bind_identifier(Identifier& id, Symbol_ptr symbol);
    Symbol_ptr get_module_member_symbol(MemberAccess& access);

    Object_ptr resolve_standard_overload(
        Call& call,
        Symbol_ptr overload_symbol,
        const ObjectVector& argument_types
    );

    Symbol_ptr resolve_target_symbol(Expression_ptr target);

    Object_ptr call_method(
        Call& call,
        MemberAccess& mac,
        const ObjectVector& argument_types,
        ClassType_ptr class_type
    );

    Object_ptr call_concrete_template(
        Call& call,
        TemplateAngular& concrete_template,
        const ObjectVector& argument_types
    );

    Object_ptr resolve_operator_overload(
        OperatorExpression& expr,
        const std::string& operator_name,
        const ObjectVector& operand_types
    );

    // =========================================================================
    // Type Annotation Visitors
    // =========================================================================

    Object_ptr visit(const TypeAnnotation_ptr type_node);
    ObjectVector visit(const TypeAnnotationVector& type_nodes);

    Object_ptr visit(NoneTypeNode& expr);
    Object_ptr visit(NativeTypeNode& expr);
    Object_ptr visit(TypeIdentifierNode& expr);
    Object_ptr visit(LiteralTypeNode& expr);
    Object_ptr visit(ListTypeNode& expr);
    Object_ptr visit(TupleTypeNode& expr);
    Object_ptr visit(SetTypeNode& expr);
    Object_ptr visit(MapTypeNode& expr);
    Object_ptr visit(VariantTypeNode& expr);
    Object_ptr visit(FunctionTypeNode& expr);
    Object_ptr visit(RecordTypeNode& expr);
    Object_ptr visit(TemplateAngularTypeNode& node);
};
} // namespace Wasp
