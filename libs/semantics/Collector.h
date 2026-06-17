#pragma once

#include "AST.h"
#include "Phase.h"
#include "Statement.h"
#include "SymbolScope.h"
#include "Type.h"
#include "TypeNode.h"

#include <map>
#include <tuple>
#include <vector>

namespace Wasp
{

class Collector : public Phase
{
public:
    explicit Collector() : Phase()
    {
    }

    using Phase::visit;

    std::map<Symbol_ptr, Statement_ptr> get_forest() const
    {
        return forest;
    }

    std::map<Symbol_ptr, SymbolScope_ptr> get_scope_forest() const
    {
        return scope_forest;
    }

private:
    // Forest

    std::map<Symbol_ptr, Statement_ptr> forest;
    std::map<Symbol_ptr, SymbolScope_ptr> scope_forest;

    // Statements

    void visit(Import& statement);

    void visit(FunctionDefinition& statement);
    void visit(MethodDefinition& statement);
    void visit(OperatorDefinition& statement);
    void visit(ClassDefinition& statement);
    void visit(TraitDefinition& statement);
    void visit(PrimitiveDefinition& statement);
    void visit(EnumDefinition& statement);
    void visit(TypeAliasDefinition& statement);

    // Types

    Type_ptr visit(const TypeNode_ptr type_node);
    TypeVector visit(const TypeNodeVector& type_nodes);

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

    // Utils

    std::tuple<Statement_ptr, SymbolScope_ptr> get_tree(Symbol_ptr symbol);

    Signature_ptr extract_signature(FunctionDefinition& def);
    Signature_ptr extract_signature(MethodDefinition& def);
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

    void merge_trait_methods(
        TypeDefinition& target_def,
        OopsType_ptr target_type,
        SymbolScope_ptr definition_scope
    );

    void merge_trait_methods(
        TypeDefinition& target_def,
        OopsType_ptr target_type,
        TraitDefinition& trait_def,
        SymbolScope_ptr definition_scope
    );
};

} // namespace Wasp
