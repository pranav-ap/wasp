#include "AST.h"
#include "Expression.h"
#include "Statement.h"
#include "SymbolScope.h"
#include "Type.h"
#include "TypeNode.h"
#include "TypeSystem.h"
#include "Workspace.h"

#include <map>
#include <memory>
#include <vector>

namespace Wasp
{
class SemanticsAnalyzer
{
public:
    explicit SemanticsAnalyzer(Workspace_ptr workspace)
        : workspace(workspace), type_system(std::make_shared<TypeSystem>())
    {
    }

    void run(std::vector<Module_ptr>& build_order);

private:
    SymbolScope_ptr current_scope;
    Module_ptr current_module;
    TypeSystem_ptr type_system;
    Workspace_ptr workspace;

private:
    // Forest
    std::map<Symbol_ptr, Statement_ptr> forest;
    std::map<Symbol_ptr, SymbolScope_ptr> scope_forest;

private:
    // Types

    Type_ptr visit(TypeNode_ptr type_node);
    TypeVector visit(TypeNodeVector& type_nodes);

    Type_ptr visit(NoneTypeNode& type_node);
    Type_ptr visit(LiteralTypeNode& type_node);
    Type_ptr visit(TypeIdentifierNode& type_node);

    Type_ptr visit(ListTypeNode& type_node);
    Type_ptr visit(TupleTypeNode& type_node);
    Type_ptr visit(SetTypeNode& type_node);
    Type_ptr visit(MapTypeNode& type_node);

    Type_ptr visit(VariantTypeNode& type_node);
    Type_ptr visit(IntersectionTypeNode& type_node);

    Type_ptr visit(FunctionTypeNode& type_node);
    Type_ptr visit(AngularTypeNode& type_node);

private:
    // Expressions

    Type_ptr visit(Expression_ptr expression);
    TypeVector visit(ExpressionVector& expressions);

    Type_ptr visit(TernaryExpression& expr);

    Type_ptr visit(IntegerLiteral& expr);
    Type_ptr visit(FloatLiteral& expr);
    Type_ptr visit(StringLiteral& expr);
    Type_ptr visit(BooleanLiteral& expr);
    Type_ptr visit(NoneLiteral& expr);

    Type_ptr visit(InterpolatedString& expr);

    Type_ptr visit(ListLiteral& expr);
    Type_ptr visit(TupleLiteral& expr);
    Type_ptr visit(MapLiteral& expr);
    Type_ptr visit(SetLiteral& expr);

    Type_ptr visit(Prefix& expr);
    Type_ptr visit(Infix& expr);

private:
    // Constructor

    Type_ptr visit(Constructor& expr);

private:
    // Call

    Type_ptr visit(Call& expr);

    Type_ptr handle_call(
        Call& call,
        Identifier& identifier,
        const TypeVector& generic_types,
        const TypeVector& argument_types
    );

    Type_ptr handle_call(
        Call& call,
        MemberAccess& access,
        TypeVector& generic_types,
        TypeVector& argument_types
    );

private:
    // Variables

    Type_ptr visit(Binding& binding);
    Type_ptr visit(Assignment& expr);

    Type_ptr visit(Identifier& expr);
    Type_ptr visit(MemberAccess& expr);

    Type_ptr mutate_variable(
        Expression_ptr identifier_expr,
        Expression_ptr assigned_expr
    );

    Type_ptr mutate_member(Expression_ptr lhs_expr, Expression_ptr rhs_expr);

private:
    // Statements

    void visit(Block& block);
    void visit(Statement_ptr statement);

    void visit(FunctionDefinition& statement);
    void visit(MethodDefinition& statement);
    void visit(OperatorDefinition& statement);
    void visit(ClassDefinition& statement);
    void visit(TraitDefinition& statement);
    void visit(PrimitiveDefinition& statement);
    void visit(EnumDefinition& statement);
    void visit(TypeAliasDefinition& statement);

    void visit(Branch& statement);
    void visit(SimpleLoop& statement);
    void visit(ForInLoop& statement);

    void visit(Return& statement);

    void visit(ExpressionStatement& statement);

private:
    // Hoist
    void hoist(Block& block);
    void hoist(Statement_ptr statement);

    void hoist(FunctionDefinition& statement);
    void hoist(MethodDefinition& statement);
    void hoist(OperatorDefinition& statement);

    void hoist(ClassDefinition& statement);
    void hoist(TraitDefinition& statement);
    void hoist(PrimitiveDefinition& statement);
    void hoist(EnumDefinition& statement);
    void hoist(TypeAliasDefinition& statement);

private:
    // Collect

    void collect(Block& block);
    void collect(Statement_ptr statement);

    void collect(FunctionDefinition& statement);
    void collect(MethodDefinition& statement);
    void collect(OperatorDefinition& statement);
    void collect(ClassDefinition& statement);
    void collect(TraitDefinition& statement);
    void collect(PrimitiveDefinition& statement);
    void collect(EnumDefinition& statement);
    void collect(TypeAliasDefinition& statement);

    // Collect Utils

    Signature_ptr extract_signature(FunctionDefinition& def);
    Signature_ptr extract_signature(OperatorDefinition& def);

    FieldMap_ptr track_fields(FieldVector fields);
    MethodMap_ptr track_methods(MethodDefinitionVector methods);
    TypeVector track_traits(TypeNodeVector traits);

    void conform_traits(TypeDefinition& def, OopsType_ptr oop_type);

    std::vector<MethodType_ptr> collect_required_methods(OopsType_ptr target_type);
    std::vector<MethodType_ptr> collect_required_methods(TraitType_ptr trait_type);

    void validate_required_methods(
        const TypeDefinition& def,
        const std::vector<MethodType_ptr>& required_method_types
    );

    void merge_trait_methods(TypeDefinition& target_def, OopsType_ptr target_type);

    void merge_trait_methods(
        TypeDefinition& target_def,
        OopsType_ptr target_type,
        TraitDefinition& trait_def
    );

    TemplateType_ptr create_template_type(FieldVector& generics);

private:
    void import_symbols(Import& statement);
    void init_module(Module_ptr current_module);

private:
    // Utils
    Statement_ptr get_tree(Symbol_ptr symbol);

    void enter_scope(ScopeType scope_type);
    void leave_scope();
};
} // namespace Wasp
