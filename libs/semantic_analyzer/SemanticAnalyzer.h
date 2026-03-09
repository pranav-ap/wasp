#pragma once

#include "Expression.h"
#include "NativeRegistry.h"
#include "Objects.h"
#include "Statement.h"
#include "SymbolScope.h"
#include "TypeAnnotation.h"
#include "TypeSystem.h"

#include <memory>
#include <string>
#include <vector>

namespace Wasp {
class SemanticAnalyzer {
  TypeSystem_ptr type_system;
  NativeRegistry_ptr native_registry;
  SymbolScope_ptr current_scope;
  ObjectVector return_type_stack;

  // ========================================================================
  // Statement Visitors
  // ========================================================================

  void visit(const Statement_ptr statement);
  void visit(std::vector<Statement_ptr> &statements);

  void visit(ExpressionStatement &statement);

  void visit(AliasDefinition &statement);
  void visit(EnumDefinition &statement);
  void visit(FunctionDefinition &statement);
  void visit(ClassDefinition &statement);
  void visit(TraitDefinition &statement);
  void visit(ImplDefinition &statement);

  void visit(AnnotationDefinition &statement);

  void visit(IfBranch &statement);
  void visit(ElseBranch &statement);
  void visit(SimpleLoop &statement);
  void visit(ForInLoop &statement);
  void visit(LoopControl &statement);

  void visit(Pass &statement);
  void visit(Return &statement);

  // ------------------------------------------------------------------------
  // Variables & Assignments
  // ------------------------------------------------------------------------

  Object_ptr define_variable(Expression_ptr assignment_expr, bool is_mutable);
  Object_ptr mutate_variable(Expression_ptr lhs_expr, Expression_ptr rhs_expr);
  void visit(VariableDefinition &statement);
  Object_ptr visit(VariableDefinitionExpression &expr);
  Object_ptr visit(UntypedAssignment &expr);
  Object_ptr visit(TypedAssignment &expr);

  // ========================================================================
  // Expression Visitors
  // ========================================================================

  Object_ptr visit(const Expression_ptr expr);
  ObjectVector visit(ExpressionVector expressions);

  Object_ptr visit(int expr);
  Object_ptr visit(double expr);
  Object_ptr visit(std::string expr);
  Object_ptr visit(bool expr);

  Object_ptr visit(Identifier &expr);
  Object_ptr visit(DotLiteral &expr);

  Object_ptr visit(Prefix &expr);
  Object_ptr visit(Infix &expr);
  Object_ptr visit(Postfix &expr);

  Object_ptr visit(ListLiteral &expr);
  Object_ptr visit(TupleLiteral &expr);
  Object_ptr visit(MapLiteral &expr);
  Object_ptr visit(SetLiteral &expr);
  Object_ptr visit(RangeLiteral &expr);

  Object_ptr visit(TypePattern &expr);

  Object_ptr visit(IfTernaryBranch &expr);
  Object_ptr visit(ElseTernaryBranch &expr);

  Object_ptr visit(Call &expr);

  // ========================================================================
  // Type Visitors
  // ========================================================================

  Object_ptr visit(const TypeAnnotation_ptr type_node);
  ObjectVector visit(std::vector<TypeAnnotation_ptr> &type_nodes);

  // Primitive Types
  Object_ptr visit(AnyTypeNode &expr);
  Object_ptr visit(NoneTypeNode &expr);

  Object_ptr visit(IntTypeNode &expr);
  Object_ptr visit(FloatTypeNode &expr);
  Object_ptr visit(StringTypeNode &expr);
  Object_ptr visit(BoolTypeNode &expr);

  Object_ptr visit(IntLiteralTypeNode &expr);
  Object_ptr visit(FloatLiteralTypeNode &expr);
  Object_ptr visit(StringLiteralTypeNode &expr);
  Object_ptr visit(BoolLiteralTypeNode &expr);

  Object_ptr visit(TypeIdentifierNode &expr);

  Object_ptr visit(ListTypeNode &expr);
  Object_ptr visit(TupleTypeNode &expr);
  Object_ptr visit(SetTypeNode &expr);
  Object_ptr visit(MapTypeNode &expr);
  Object_ptr visit(VariantTypeNode &expr);
  Object_ptr visit(FunctionTypeNode &expr);
  Object_ptr visit(RecordTypeNode &expr);

  // ========================================================================
  // Utils
  // ========================================================================

  void enter_scope(ScopeType scope_type);
  void leave_scope();

  void register_natives();

public:
  SemanticAnalyzer(NativeRegistry_ptr native_registry)
      : type_system(std::make_shared<TypeSystem>()),
        native_registry(native_registry) {};

  void run(struct Module &ast);
};

using SemanticAnalyzer_ptr = std::unique_ptr<SemanticAnalyzer>;

} // namespace Wasp