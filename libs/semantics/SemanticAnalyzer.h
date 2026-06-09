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

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace Wasp
{

class SemanticAnalyzer
{
public:
    SemanticAnalyzer(Workspace_ptr workspace)
        : type_system(std::make_shared<TypeSystem>(workspace->pool)),
          salt(std::make_shared<Salt>()), workspace(workspace) {};

    void run(const std::vector<Module_ptr>& build_order);

private:
    Workspace_ptr workspace;
    TypeSystem_ptr type_system;
    Salt_ptr salt;
    Module_ptr current_module = nullptr;
    SymbolScope_ptr current_scope;
    ObjectVector return_type_stack;

    std::map<Symbol_ptr, Statement_ptr> forest;
    std::map<Symbol_ptr, SymbolScope_ptr> scope_forest;

    StatementVector pending_templates;

    // Scope & Module
    void enter_scope(ScopeType scope_type);
    void leave_scope();
    void leave_scope_keep_symbol(Symbol_ptr symbol_to_keep);
    void setup_exports(Module_ptr mod, StringVector ordered_export_names);
    StringVector setup_ordered_export_names(Module_ptr mod);
    void extract_module_type(Module_ptr mod);

    // Main Visitors
    void visit(const Statement_ptr statement);
    void visit(StatementVector& statements);

    void visit(ExpressionStatement& statement);
    void visit(FunctionDefinition& statement);
    void visit(MethodDefinition& statement);
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

    Object_ptr visit(Expression_ptr expr);
    ObjectVector visit(ExpressionVector expressions);

    Object_ptr visit(Identifier& expr);
    Object_ptr visit(MemberAccess& expr);
    Object_ptr visit(TemplateAngular& template_instantiation);

    Object_ptr visit(IntegerLiteral& expr);
    Object_ptr visit(FloatLiteral& expr);
    Object_ptr visit(StringLiteral& expr);
    Object_ptr visit(BooleanLiteral& expr);
    Object_ptr visit(NoneLiteral& expr);

    Object_ptr visit(ListLiteral& expr);
    Object_ptr visit(TupleLiteral& expr);
    Object_ptr visit(MapLiteral& expr);
    Object_ptr visit(SetLiteral& expr);

    Object_ptr visit(Assignment& expr);

    Object_ptr visit(IfTernaryBranch& expr);
    Object_ptr visit(ElseTernaryBranch& expr);

    Object_ptr visit(Call& expr);

    void desugar_member_access(Expression_ptr expr);
    Object_ptr desugar_interpolated_string(const Expression_ptr& expr);

    Object_ptr visit(Constructor& expr);

    Object_ptr visit(Prefix& expr);
    Object_ptr visit(Infix& expr);
    Object_ptr visit(Postfix& expr);

    Object_ptr visit(const TypeAnnotation_ptr type_node);
    ObjectVector visit(const TypeAnnotationVector& type_nodes);

    Object_ptr visit(NoneTypeNode& expr);
    Object_ptr visit(TypeIdentifierNode& expr);
    Object_ptr visit(LiteralTypeNode& expr);
    Object_ptr visit(ListTypeNode& expr);
    Object_ptr visit(TupleTypeNode& expr);
    Object_ptr visit(SetTypeNode& expr);
    Object_ptr visit(MapTypeNode& expr);
    Object_ptr visit(VariantTypeNode& expr);
    Object_ptr visit(IntersectionTypeNode& expr);
    Object_ptr visit(FunctionTypeNode& expr);
    Object_ptr visit(TemplateAngularTypeNode& node);

    // Passes & Hoisting
    void inject_prelude();
    void hoist_statements(StatementVector& statements);
    void hoist_names(StatementVector& statements);
    void hoist_signatures_and_store_ast(StatementVector& statements);
    void hoist(Import& stmt);
    void hoist(CallableDefinition& def);

    // Overload Management
    void define_or_add_to_overload_set(
        const std::string& name,
        Symbol_ptr new_function
    );

    void validate_unique_signature(
        const OverloadsSymbol& overload_symbol,
        const Signature_ptr& new_signature,
        const std::string& function_name
    );

    void validate_method_signature(
        const SignaturesSet_ptr& existing_signatures,
        const Signature_ptr& new_signature,
        const std::string& method_name,
        const std::string& class_name
    );

    // Call Handlers
    Object_ptr handle_identifier_call(
        Call& call,
        Identifier& id,
        const ObjectVector& argument_types
    );

    Object_ptr handle_member_call(
        Call& call,
        MemberAccess& ma,
        ObjectVector& argument_types
    );

    // Template Helpers
    void define_template_parameters(TemplateType_ptr template_type);
    void bind_template_parameters(TemplateType_ptr template_type);

    // Parameter Binding
    void bind_parameters(
        CallableDefinition& def,
        Signature_ptr signature,
        ScopeType scope_type
    );

    // Placeholder Handling
    bool handle_placeholder(CallableDefinition& def);
    std::optional<TokenType> find_placeholder(const StatementVector& body);
    void validate_native_location();

    // Overload Resolution Helpers
    void box_arguments_if_needed(
        Call& call,
        const ObjectVector& argument_types,
        const Signature_ptr& signature
    );

    int compute_runtime_overload_index(
        const SymbolVector& candidates,
        Symbol_ptr winner
    );

    ObjectVector validate_and_collect_deduced_args(
        const Signature_ptr& signature,
        const ObjectStringMap& substitutions
    );

    void replace_callable_with_specialized(
        Call& call,
        const std::string& specialized_name,
        Symbol_ptr specialized_group
    );

    // Angular Arguments Helpers
    ObjectVector resolve_angular_arguments(TemplateAngular& template_angular);
    Symbol_ptr resolve_template_target(TemplateAngular& template_angular);
    ObjectStringMap build_substitutions_from_angular_args(
        Symbol_ptr blueprint_symbol,
        const ObjectVector& angular_args
    );

    // OOP Analysis
    void analyze_oops_definition(
        AbstractOopsDefinition& def,
        OopsType_ptr oop_type
    );
    void resolve_traits(AbstractOopsDefinition& def, OopsType_ptr oop_type);
    void fill_oops_member_names(
        AbstractOopsDefinition& def,
        OopsType_ptr oop_type
    );
    void hoist_methods(AbstractOopsDefinition& def, OopsType_ptr oop_type);
    void analyze_methods(AbstractOopsDefinition& def);
    void check_trait_conformance(OopsType_ptr oop_type);
    void inherit_default_methods(
        AbstractOopsDefinition& def,
        OopsType_ptr oop_type
    );

    std::optional<Object_ptr> try_resolve_as_enum(MemberAccess& expr);

    void validate_purity_constraints(Symbol_ptr target_symbol) const;

    Expression_ptr try_box_expression(
        Expression_ptr expr,
        Object_ptr actual_type,
        Object_ptr expected_type
    );

    // Resolvers
    Object_ptr resolve_literal(
        Resolvable& expr,
        const std::string& type_name,
        Object_ptr type_obj
    );
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

    Object_ptr call_possible_method(
        Call& call,
        MemberAccess& mac,
        const ObjectVector& argument_types,
        OopsType_ptr oops_type
    );

    Object_ptr call_native_method(
        Call& call,
        MemberAccess& mac,
        const ObjectVector& argument_types,
        std::string native_class_name
    );

    Object_ptr call_template_function(
        Call& call,
        TemplateAngular& concrete_template,
        const ObjectVector& argument_types
    );

    void analyze_callable(CallableDefinition& def, ScopeType scope_type);
    void bind_callable_parameters(
        CallableDefinition& def,
        Signature_ptr signature,
        ScopeType scope_type
    );

    // Templates & Generics
    TemplateType_ptr create_template_type(
        const FieldDefinitionVector& template_params
    );

    void analyze_template_parameter_constructor(
        GenericType_ptr template_param,
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

    Symbol_ptr monomorphize_oops_template(
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
    Object_ptr mutate_variable(
        Expression_ptr lhs_expr,
        Expression_ptr rhs_expr
    );
    Object_ptr mutate_member(Expression_ptr lhs_expr, Expression_ptr rhs_expr);
};

} // namespace Wasp
