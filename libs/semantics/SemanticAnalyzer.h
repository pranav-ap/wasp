#pragma once

#include "AST.h"
#include "Expression.h"
#include "Objects.h"
#include "Resolvable.h"
#include "Salt.h"
#include "Statement.h"
#include "SymbolScope.h"
#include "Token.h"
#include "TypeAnnotation.h"
#include "TypeSystem.h"
#include "Workspace.h"

#include <memory>
#include <optional>
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
          salt(std::make_shared<Salt>(workspace)), workspace(workspace) {};

    void run(const std::vector<Module_ptr>& build_order);

private:
    Workspace_ptr workspace;
    TypeSystem_ptr type_system;
    Salt_ptr salt;
    Module_ptr current_module = nullptr;
    SymbolScope_ptr current_scope;
    ObjectVector return_type_stack;

    StatementVector pending_templates;

    // Scope & Module
    void enter_scope(ScopeType scope_type);
    void leave_scope();
    void leave_scope_keep_symbol(Symbol_ptr symbol_to_keep);
    void extract_module_type(Module_ptr module);
    void setup_exports(Module_ptr mod, StringVector ordered_export_names);
    StringVector setup_ordered_export_names(Module_ptr mod);

    // Main Visitors
    void visit(const Statement_ptr statement);
    void visit(StatementVector& statements);

    void visit(ExpressionStatement& statement);
    void visit(FunctionDefinition& statement);
    void visit(OperatorDefinition& statement);
    void visit(ClassDefinition& statement);
    void visit(TraitDefinition& statement);
    void visit(EnumDefinition& statement);
    void visit(TypeAliasDefinition& statement);
    void visit(IfBranch& statement);
    void visit(ElseBranch& statement);
    void visit(SimpleLoop& statement);
    void visit(ForInLoop& statement);
    void visit(LoopControl& statement);
    void visit(Placeholder& statement);
    void visit(Return& statement);

    Object_ptr visit(const Expression_ptr expr);
    ObjectVector visit(ExpressionVector expressions);

    Object_ptr visit(Identifier& expr);
    Object_ptr visit(MemberAccess& expr);
    Object_ptr visit(TemplateAngular& template_instantiation);

    Object_ptr visit(Assignment& expr);

    Object_ptr visit(IfTernaryBranch& expr);
    Object_ptr visit(ElseTernaryBranch& expr);

    Object_ptr visit(Call& expr);
    Object_ptr visit(Constructor& expr);

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
    Object_ptr visit(IntersectionTypeNode& expr);
    Object_ptr visit(FunctionTypeNode& expr);
    Object_ptr visit(RecordTypeNode& expr);
    Object_ptr visit(TemplateAngularTypeNode& node);

    // Passes & Hoisting
    void inject_prelude();
    void hoist_statements(StatementVector& statements);
    void hoist_names(StatementVector& statements);
    void hoist_signatures(StatementVector& statements);
    void hoist_import(Import& stmt);
    void hoist_function_definition(AbstractCallable& def);

    // OOP Analysis
    void analyze_oops_definition(AbstractOopsDefinition& def, OopsType_ptr oop_type);
    void resolve_traits(AbstractOopsDefinition& def, OopsType_ptr oop_type);
    void fill_oops_member_names(AbstractOopsDefinition& def, OopsType_ptr oop_type);
    void hoist_methods(AbstractOopsDefinition& def, OopsType_ptr oop_type);
    void analyze_methods(AbstractOopsDefinition& def);
    void check_trait_conformance(OopsType_ptr oop_type);
    void inherit_default_methods(AbstractOopsDefinition& def, OopsType_ptr oop_type);

    StringVector unfurl_member_access(const MemberAccess& expr);
    std::optional<Object_ptr> try_resolve_as_enum(MemberAccess& expr);

    // Callable Analysis
    void analyze_callable(
        AbstractCallable& def,
        ScopeType scope_type,
        Object_ptr context_type,
        bool is_static
    );
    void validate_purity_constraints(Symbol_ptr target_symbol) const;

    Expression_ptr try_box_expression(
        Expression_ptr expr,
        Object_ptr actual_type,
        Object_ptr expected_type
    );

    // Resolvers
    Object_ptr resolve_literal(Resolvable& expr, const std::string& type_name, Object_ptr type_obj);
    Symbol_ptr resolve_target_symbol(Expression_ptr target);
    Object_ptr resolve_member_access(
        MemberAccess& expr,
        Object_ptr target_type,
        const std::string& member_name
    );
    Object_ptr resolve_standard_overload(
        Call& call,
        Symbol_ptr overload_symbol,
        const ObjectVector& argument_types
    );
    Symbol_ptr get_module_member_symbol(MemberAccess& access);
    void bind_identifier(Identifier& id, Symbol_ptr symbol);

    // Call Execution Evaluators
    Object_ptr call_method(
        Call& call,
        MemberAccess& mac,
        const ObjectVector& argument_types,
        OopsType_ptr oops_type
    );
    Object_ptr call_template_function(
        Call& call,
        TemplateAngular& concrete_template,
        const ObjectVector& argument_types
    );

    // Templates & Generics
    std::pair<ObjectStringMap, StringVector> evaluate_template_params(
        const std::vector<FieldDefinition>& template_params
    );
    void analyze_template_parameter_constructor(
        TemplateParameterType_ptr template_param,
        const ObjectVector& argument_types
    );
    Object_ptr resolve_implicit_template(
        Call& call,
        Symbol_ptr function_symbol,
        Signature_ptr signature,
        const ObjectVector& argument_types
    );
    Symbol_ptr monomorphize_callable_template(
        Symbol_ptr blueprint_symbol,
        const ObjectStringMap& substitutions,
        const std::string& specialized_name
    );
    Symbol_ptr monomorphize_class_template(
        Symbol_ptr blueprint_symbol,
        const ObjectStringMap& substitutions,
        const std::string& specialized_name
    );
    std::optional<Object_ptr> try_monomorphize_operator(
        OperatorExpression& expr,
        Symbol_ptr function_symbol,
        Signature_ptr signature,
        const ObjectVector& operand_types
    );

    // Variables & Mutation
    Object_ptr define_variable(Assignment& assign);
    Object_ptr mutate_variable(Expression_ptr lhs_expr, Expression_ptr rhs_expr);
    Object_ptr mutate_member(Expression_ptr lhs_expr, Expression_ptr rhs_expr);

    // Desugaring
    void desugar_literal(
        const Expression_ptr& expr,
        const std::string& type_alias_name
    );
    Object_ptr desugar_interpolated_string(const Expression_ptr& expr);
    void desugar_overloaded_operator(
        const Expression_ptr& expr,
        Symbol_ptr operator_symbol,
        int overload_index,
        const std::vector<Expression_ptr>& arguments
    );

    void desugar_call(Expression_ptr expr);
    void desugar_member_access(Expression_ptr expr);

    // Type Utilities

    std::string get_operator_symbol_name(TokenType fixity, TokenType op_type);
};

} // namespace Wasp
