#include "AST.h"
#include "Expression.h"
#include "Statement.h"
#include "SymbolScope.h"
#include "Type.h"
#include "TypeNode.h"
#include "TypeSystem.h"
#include "Workspace.h"

#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace Wasp
{

using TypeSubstitutionMap = std::map<std::string, Type_ptr>;

struct FunctionCandidate
{
    Symbol_ptr symbol;
    int index;
    FunctionType_ptr function_type;
};

using FunctionCandidateVector = std::vector<FunctionCandidate>;

struct MethodCandidate
{
    MethodType_ptr method_type;
    int index;
};

using MethodCandidateVector = std::vector<MethodCandidate>;

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
    std::map<Symbol_ptr, std::pair<Statement_ptr, SymbolScope_ptr>> forest;

    std::pair<Statement_ptr, SymbolScope_ptr> get_tree(Symbol_ptr symbol);
    void add_tree(Symbol_ptr symbol, Statement_ptr tree, SymbolScope_ptr scope);
    bool contains_tree(Symbol_ptr symbol) const;

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

    void validate_solid_constructor(
        ClassType_ptr class_type,
        TypeVector solid_types,
        TypeVector argument_types
    );

    std::pair<Type_ptr, TypeSubstitutionMap> validate_constructor_template(
        ClassType_ptr class_type,
        TypeVector solid_types,
        TypeVector argument_types
    );

private:
    // Call

    Type_ptr visit(Call& expr);

    Type_ptr visit(
        Call& call,
        Identifier& identifier,
        const TypeVector& generic_types,
        const TypeVector& argument_types
    );

    Type_ptr visit(Call& call, MemberAccess& access, const TypeVector& argument_types);

    Type_ptr visit(
        Call& call,
        MemberAccess& ma,
        const TypeVector& argument_types,
        const OopsType_ptr owner_type
    );

    std::tuple<MethodType_ptr, int> resolve_method(
        const MethodTypeVector& method_types,
        const TypeVector& argument_types
    ) const;

    std::optional<std::pair<Symbol_ptr, int>> try_resolve_solid(
        const std::string& name,
        const std::vector<FunctionCandidate>& candidates,
        const TypeVector& argument_types
    ) const;

    std::optional<std::tuple<Symbol_ptr, int, TypeSubstitutionMap>> try_resolve_template(
        const std::string& name,
        const std::vector<FunctionCandidate>& candidates,
        const TypeVector& solid_types,
        const TypeVector& argument_types
    ) const;

    FunctionCandidate get_best_candidate(
        const std::vector<FunctionCandidate>& candidates,
        const TypeVector& argument_types
    ) const;

    std::pair<bool, TypeSubstitutionMap> is_assignable_template_function(
        FunctionType_ptr function_type,
        const TypeVector& solid_types,
        const TypeVector& argument_types
    ) const;

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
    void visit(OperatorDefinition& statement);

    void visit(MethodDefinition& statement);
    void visit(MethodDefinitionVector& statement);

    void visit(ClassDefinition& statement);
    void visit(TraitDefinition& statement);
    void visit(PrimitiveDefinition& statement);

    void visit(Branch& statement);
    void visit(SimpleLoop& statement);
    void visit(ForInLoop& statement);

    void visit(Return& statement);

    void visit(ExpressionStatement& statement);

private:
    // Hoist
    void hoist(Block& block);
    void hoist(Statement_ptr statement);

    void hoist(EnumDefinition& statement);
    void hoist(TypeAliasDefinition& statement);

    void hoist(FunctionDefinition& statement);
    void hoist(OperatorDefinition& statement);

    void hoist(MethodDefinitionVector& def);
    void hoist(MethodDefinition& def);

    void hoist(ClassDefinition& statement);
    void hoist(TraitDefinition& statement);
    void hoist(PrimitiveDefinition& statement);

private:
    // Collect

    void collect(Block& block);
    void collect(Statement_ptr statement);

    void collect(EnumDefinition& statement);
    void collect(TypeAliasDefinition& statement);

    void collect(FunctionDefinition& statement);
    void collect(OperatorDefinition& statement);

    void collect(ClassDefinition& statement);
    void collect(TraitDefinition& statement);
    void collect(PrimitiveDefinition& statement);

    // Collect Utils

    TemplateType_ptr create_template_type(FieldVector& generics);

    void validate_new_function_type(Symbol_ptr);
    void validate_new_function_type_friends(Symbol_ptr, Symbol_ptr, FunctionType_ptr);
    void shadow_new_function_type_parents(Symbol_ptr, Symbol_ptr, FunctionType_ptr);

    FieldMap_ptr collect(FieldVector& fields);
    MethodType_ptr collect(MethodDefinition& statement, Symbol_ptr owner_symbol);
    MethodMap_ptr collect(MethodDefinitionVector& methods, Symbol_ptr owner_symbol);

    void conform_to_traits(TypeDefinition& def, OopsType_ptr oop_type);

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

private:
    // Exports

    void import_symbols(Import& statement);
    void init_module(Module_ptr current_module);

private:
    // Utils

    void enter_scope(ScopeType scope_type);
    void leave_scope();
};
} // namespace Wasp
