#include "AST.h"
#include "Expression.h"
#include "Statement.h"
#include "SymbolScope.h"
#include "Type.h"
#include "TypeAnnotation.h"
#include "TypeSystem.h"
#include "Workspace.h"

#include <memory>
#include <vector>

namespace Wasp
{

class TypeCollection

{
public:
    explicit TypeCollection()
        : current_scope(nullptr),
          type_system(std::make_shared<TypeSystem>())
    {
    }

    void run(Module_ptr mod);

private:
    Workspace_ptr workspace;
    Module_ptr current_module;
    SymbolScope_ptr current_scope;
    TypeSystem_ptr type_system;

    void enter_scope(ScopeType scope_type);
    void leave_scope();

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

    void visit(Branch& statement);
    void visit(SimpleLoop& statement);
    void visit(ForInLoop& statement);

    // Expressions

    void visit(std::vector<Expression_ptr>& expressions);
    void visit(Expression_ptr expression);

    void visit(Binding& binding);

    // Types

    Type_ptr visit(const TypeAnnotation_ptr type_node);
    TypeVector visit(const TypeAnnotationVector& type_nodes);

    Type_ptr visit(NoneTypeNode& expr);
    Type_ptr visit(TypeIdentifierNode& expr);

    Type_ptr visit(ListTypeNode& expr);
    Type_ptr visit(TupleTypeNode& expr);
    Type_ptr visit(SetTypeNode& expr);
    Type_ptr visit(MapTypeNode& expr);

    Type_ptr visit(VariantTypeNode& expr);
    Type_ptr visit(IntersectionTypeNode& expr);

    Type_ptr visit(FunctionTypeNode& expr);
    Type_ptr visit(AngularTypeNode& node);

    // Utils

    TemplateType_ptr create_template_type(
        const FieldVector& generics
    );
    void define_template_type(TemplateType_ptr template_type);

    Signature_ptr analyze(FunctionDefinition& def);

    FieldMap_ptr track_fields(FieldVector fields);
    MethodMap_ptr track_methods(FunctionDefinitionVector methods);
    TypeVector track_traits(TypeAnnotationVector traits);
};

} // namespace Wasp
