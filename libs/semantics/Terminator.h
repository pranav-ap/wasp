#pragma once

#include "AST.h"
#include "Expression.h"
#include "Phase.h"
#include "Statement.h"
#include "SymbolScope.h"
#include "Type.h"
#include "TypeNode.h"
#include "Workspace.h"

namespace Wasp
{

class Terminator : public Phase
{
public:
    explicit Terminator(
        Workspace_ptr workspace,
        Module_ptr current_module,
        SymbolScope_ptr current_scope
    )
        : Phase(workspace, current_module, current_scope)
    {
    }

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

    // Expressions

    Type_ptr visit(Expression_ptr expression);
    TypeVector visit(ExpressionVector& expressions);

    Type_ptr visit(Binding& binding);
    Type_ptr visit(Assignment& expr);

    Type_ptr visit(TernaryExpression& expr);

    Type_ptr visit(Identifier& expr);
    Type_ptr visit(MemberAccess& expr);

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

    Type_ptr visit(Call& expr);
    Type_ptr visit(Constructor& expr);

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

    Type_ptr mutate_variable(
        Expression_ptr identifier_expr,
        Expression_ptr assigned_expr
    );

    Type_ptr mutate_member(Expression_ptr lhs_expr, Expression_ptr rhs_expr);

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
};

} // namespace Wasp
