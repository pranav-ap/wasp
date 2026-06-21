#pragma once

#include "AST.h"
#include "Expression.h"
#include "Statement.h"
#include "TypeNode.h"

#include <unordered_map>

namespace Wasp
{

class ASTCloner
{
public:
    static ASTCloner& get()
    {
        static ASTCloner instance;
        return instance;
    }

    ASTCloner(const ASTCloner&) = delete;
    ASTCloner& operator=(const ASTCloner&) = delete;
    ASTCloner(ASTCloner&&) = delete;
    ASTCloner& operator=(ASTCloner&&) = delete;

    Statement_ptr clone(const Statement_ptr& stmt);
    Expression_ptr clone(const Expression_ptr& expr);
    TypeNode_ptr clone(const TypeNode_ptr& type);

    Block clone_block(const Block& block);

    FieldVector clone(const FieldVector& fields);
    FunctionDefinitionVector clone(const FunctionDefinitionVector& funcs);
    MethodDefinitionVector clone(const MethodDefinitionVector& methods);

    ExpressionVector clone(const ExpressionVector& expressions);
    TypeNodeVector clone(const TypeNodeVector& types);
    Statement_ptr clone(const Import& stmt);
    Statement_ptr clone(const Block& stmt);
    Statement_ptr clone(const ExpressionStatement& stmt);
    Statement_ptr clone(const TypeAliasDefinition& stmt);
    Statement_ptr clone(const EnumDefinition& stmt);
    Statement_ptr clone(const FunctionDefinition& stmt);
    Statement_ptr clone(const MethodDefinition& stmt);
    Statement_ptr clone(const OperatorDefinition& stmt);
    Statement_ptr clone(const RecordDefinition& stmt);
    Statement_ptr clone(const ClassDefinition& stmt);
    Statement_ptr clone(const TraitDefinition& stmt);
    Statement_ptr clone(const PrimitiveDefinition& stmt);
    Statement_ptr clone(const Branch& stmt);
    Statement_ptr clone(const SimpleLoop& stmt);
    Statement_ptr clone(const ForInLoop& stmt);
    Statement_ptr clone(const LoopControl& stmt);
    Statement_ptr clone(const Return& stmt);
    Statement_ptr clone(const Pass& stmt);
    Statement_ptr clone(const Required& stmt);
    Statement_ptr clone(const Native& stmt);

    // Expression cloning - all named 'clone'
    Expression_ptr clone(const IntegerLiteral& expr);
    Expression_ptr clone(const FloatLiteral& expr);
    Expression_ptr clone(const StringLiteral& expr);
    Expression_ptr clone(const BooleanLiteral& expr);
    Expression_ptr clone(const NoneLiteral& expr);
    Expression_ptr clone(const InterpolatedString& expr);
    Expression_ptr clone(const Range& expr);
    Expression_ptr clone(const Identifier& expr);
    Expression_ptr clone(const MemberAccess& expr);
    Expression_ptr clone(const Call& expr);
    Expression_ptr clone(const Pipe& expr);
    Expression_ptr clone(const Constructor& expr);
    Expression_ptr clone(const Prefix& expr);
    Expression_ptr clone(const Infix& expr);
    Expression_ptr clone(const ListLiteral& expr);
    Expression_ptr clone(const TupleLiteral& expr);
    Expression_ptr clone(const MapLiteral& expr);
    Expression_ptr clone(const SetLiteral& expr);
    Expression_ptr clone(const Binding& expr);
    Expression_ptr clone(const Assignment& expr);
    Expression_ptr clone(const TernaryExpression& expr);

    TypeNode_ptr clone(const NoneTypeNode& type);
    TypeNode_ptr clone(const LiteralTypeNode& type);
    TypeNode_ptr clone(const TypeIdentifierNode& type);
    TypeNode_ptr clone(const ListTypeNode& type);
    TypeNode_ptr clone(const TupleTypeNode& type);
    TypeNode_ptr clone(const SetTypeNode& type);
    TypeNode_ptr clone(const MapTypeNode& type);
    TypeNode_ptr clone(const VariantTypeNode& type);
    TypeNode_ptr clone(const IntersectionTypeNode& type);
    TypeNode_ptr clone(const FunctionTypeNode& type);
    TypeNode_ptr clone(const AngularTypeNode& type);

private:
    ASTCloner() = default;

    std::unordered_map<Symbol_ptr, Symbol_ptr> symbol_cache_;
    std::unordered_map<TypeNode_ptr, TypeNode_ptr> type_cache_;
};

} // namespace Wasp
