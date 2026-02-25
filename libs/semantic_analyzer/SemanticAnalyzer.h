#pragma once

#include "Objects.h"
#include "TypeSystem.h"
#include "Statement.h"
#include "Expression.h"
#include "TypeAnnotation.h"
#include "SymbolScope.h"
#include <memory>
#include <tuple>
#include <optional>
#include <vector>
#include <string>

namespace Wasp {
    
class SemanticAnalyzer
{
    int next_id;

    TypeSystem_ptr type_system;
    SymbolScope_ptr current_scope;

    // ========================================================================
    // Statement Visitors
    // ========================================================================

    void visit(const Statement_ptr statement);
    void visit(std::vector<Statement_ptr>& statements);

    void visit(ExpressionStatement& statement);

    void visit(VariableDefinition& statement);
    void visit(AliasDefinition& statement);
    void visit(EnumDefinition& statement);
    void visit(FunctionDefinition& statement);
    void visit(ClassDefinition& statement);
    void visit(TraitDefinition& statement);
    void visit(ImplDefinition& statement);

    void visit(AnnotationDefinition& statement);

    void visit(IfBranch& statement);
    void visit(ElseBranch& statement);
    void visit(SimpleLoop& statement);
    void visit(ForInLoop& statement);
    void visit(LoopControl& statement);
    
    void visit(Pass& statement);
    void visit(Return& statement);

    // ========================================================================
    // Expression Visitors
    // ========================================================================

    Object_ptr visit(const Expression_ptr expr);
    ObjectVector visit(ExpressionVector expressions);

    Object_ptr visit(int expr);
    Object_ptr visit(double expr);
    Object_ptr visit(std::string expr); 
    Object_ptr visit(bool expr);
    
    Object_ptr visit(Identifier& expr);
    Object_ptr visit(DotLiteral& expr);
    Object_ptr visit(DotDotLiteral& expr);
    Object_ptr visit(DotDotDotLiteral& expr);

    Object_ptr visit(Prefix& expr);
    Object_ptr visit(Infix& expr);
    Object_ptr visit(Postfix& expr);
    
    Object_ptr visit(ListLiteral& expr);
    Object_ptr visit(TupleLiteral& expr);
    Object_ptr visit(MapLiteral& expr);
    Object_ptr visit(SetLiteral& expr);
    Object_ptr visit(RangeLiteral& expr);

    Object_ptr visit(VariableDefinitionExpression& expr);
    Object_ptr visit(UntypedAssignment& expr);
    Object_ptr visit(TypedAssignment& expr);
    Object_ptr visit(TypePattern& expr);

    Object_ptr visit(IfTernaryBranch& expr);
    Object_ptr visit(ElseTernaryBranch& expr);

    Object_ptr visit(Call& expr);

    Object_ptr infer_chain_member_type(Object_ptr lhs_operand_type, Expression_ptr expr, bool null_check_access);

    // ========================================================================
    // Type Visitors
    // ========================================================================

    Object_ptr visit(const TypeAnnotation_ptr type_node);
    ObjectVector visit(std::vector<TypeAnnotation_ptr>& type_nodes);

    // Primitive Types
    Object_ptr visit(AnyTypeNode& expr);
    Object_ptr visit(NoneTypeNode& expr);

    Object_ptr visit(IntTypeNode& expr);
    Object_ptr visit(FloatTypeNode& expr);
    Object_ptr visit(StringTypeNode& expr);
    Object_ptr visit(BoolTypeNode& expr);

    // Literal Types
    Object_ptr visit(IntLiteralTypeNode& expr);
    Object_ptr visit(FloatLiteralTypeNode& expr);
    Object_ptr visit(StringLiteralTypeNode& expr);
    Object_ptr visit(BoolLiteralTypeNode& expr);

    // Identifier Type 
    Object_ptr visit(TypeIdentifierNode& expr);
    
    // Composite Types
    Object_ptr visit(ListTypeNode& expr);
    Object_ptr visit(TupleTypeNode& expr);
    Object_ptr visit(SetTypeNode& expr);
    Object_ptr visit(MapTypeNode& expr);
    Object_ptr visit(VariantTypeNode& expr);
    Object_ptr visit(FunctionTypeNode& expr);
    Object_ptr visit(RecordTypeNode& expr);

    // ========================================================================
    // Utils
    // ========================================================================

    void enter_scope(ScopeType scope_type);
    void leave_scope();

    std::tuple<std::string, Object_ptr> deconstruct_type_pattern(Expression_ptr expression);

    bool any_eq(ObjectVector vec, Object_ptr x);
    ObjectVector remove_duplicates(ObjectVector vec);

public:
    SemanticAnalyzer()
        : next_id(10),
        type_system(std::make_shared<TypeSystem>()) {};

    void run(struct Module& ast); 
};

using SemanticAnalyzer_ptr = std::unique_ptr<SemanticAnalyzer>;

}