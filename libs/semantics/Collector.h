#include "AST.h"
#include "Statement.h"
#include "SymbolScope.h"
#include "Type.h"
#include "TypeNode.h"
#include "TypeSystem.h"
#include "Workspace.h"

#include <memory>

namespace Wasp
{

class Collector
{
public:
    explicit Collector()
        : current_scope(nullptr),
          type_system(std::make_shared<TypeSystem>())
    {
    }

    void run(Module_ptr mod);

private:
    Module_ptr current_module;
    SymbolScope_ptr current_scope;
    TypeSystem_ptr type_system;

    // Statements

    void visit(Block& block);
    void visit(Statement_ptr statement);

    void visit(Import& statement);

    void visit(FunctionDefinition& statement);
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

    void enter_scope(ScopeType scope_type);
    void leave_scope();

    Signature_ptr analyze(FunctionDefinition& def);
    Signature_ptr analyze(OperatorDefinition& def);

    FieldMap_ptr track_fields(FieldVector fields);
    MethodMap_ptr track_methods(FunctionDefinitionVector methods);
    TypeVector track_traits(TypeNodeVector traits);
};

} // namespace Wasp
