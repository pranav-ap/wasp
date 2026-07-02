#pragma once

#include "AST.h"
#include "Expression.h"
#include "Statement.h"
#include "TypeNode.h"
#include "nlohmann/json_fwd.hpp"
#include <nlohmann/json.hpp>
#include <string>

namespace Wasp
{

class ASTPrinter
{
public:
    static ASTPrinter& get()
    {
        static ASTPrinter instance;
        return instance;
    }

    ASTPrinter(const ASTPrinter&) = delete;
    ASTPrinter& operator=(const ASTPrinter&) = delete;
    ASTPrinter(ASTPrinter&&) = delete;
    ASTPrinter& operator=(ASTPrinter&&) = delete;

    nlohmann::json print(const Statement_ptr& stmt);
    nlohmann::json print(const Expression_ptr& expr);
    nlohmann::json print(const TypeNode_ptr& type);

    nlohmann::json print(const Block& block);
    nlohmann::json print(const FieldVector& fields);
    nlohmann::json print(const FunctionDefinitionVector& funcs);
    nlohmann::json print(const MethodDefinitionVector& methods);
    nlohmann::json print(const ExpressionVector& expressions);
    nlohmann::json print(const TypeNodeVector& types);

    // Statements
    nlohmann::json print(const Import& stmt);
    nlohmann::json print(const ExpressionStatement& stmt);
    nlohmann::json print(const TypeAliasDefinition& stmt);
    nlohmann::json print(const EnumDefinition& stmt);
    nlohmann::json print(const FunctionDefinition& stmt);
    nlohmann::json print(const MethodDefinition& stmt);
    nlohmann::json print(const OperatorDefinition& stmt);
    nlohmann::json print(const RecordDefinition& stmt);
    nlohmann::json print(const ClassDefinition& stmt);
    nlohmann::json print(const TraitDefinition& stmt);
    nlohmann::json print(const PrimitiveDefinition& stmt);
    nlohmann::json print(const Branch& stmt);
    nlohmann::json print(const SimpleLoop& stmt);
    nlohmann::json print(const ForInLoop& stmt);
    nlohmann::json print(const LoopControl& stmt);
    nlohmann::json print(const Return& stmt);
    nlohmann::json print(const Pass& stmt);
    nlohmann::json print(const Required& stmt);
    nlohmann::json print(const Native& stmt);

    // Expressions
    nlohmann::json print(const IntegerLiteral& expr);
    nlohmann::json print(const FloatLiteral& expr);
    nlohmann::json print(const StringLiteral& expr);
    nlohmann::json print(const BooleanLiteral& expr);
    nlohmann::json print(const NoneLiteral& expr);
    nlohmann::json print(const InterpolatedString& expr);
    nlohmann::json print(const Range& expr);
    nlohmann::json print(const Identifier& expr);
    nlohmann::json print(const MemberAccess& expr);
    nlohmann::json print(const Call& expr);
    nlohmann::json print(const Pipe& expr);
    nlohmann::json print(const Constructor& expr);
    nlohmann::json print(const Prefix& expr);
    nlohmann::json print(const Infix& expr);
    nlohmann::json print(const ListLiteral& expr);
    nlohmann::json print(const TupleLiteral& expr);
    nlohmann::json print(const MapLiteral& expr);
    nlohmann::json print(const SetLiteral& expr);
    nlohmann::json print(const Binding& expr);
    nlohmann::json print(const Assignment& expr);
    nlohmann::json print(const TernaryExpression& expr);

    // Types
    nlohmann::json print(const NoneTypeNode& type);
    nlohmann::json print(const LiteralTypeNode& type);
    nlohmann::json print(const TypeIdentifierNode& type);
    nlohmann::json print(const ListTypeNode& type);
    nlohmann::json print(const TupleTypeNode& type);
    nlohmann::json print(const SetTypeNode& type);
    nlohmann::json print(const MapTypeNode& type);
    nlohmann::json print(const VariantTypeNode& type);
    nlohmann::json print(const IntersectionTypeNode& type);
    nlohmann::json print(const FunctionTypeNode& type);
    nlohmann::json print(const AngularTypeNode& type);

private:
    ASTPrinter() = default;

    nlohmann::json make_node(const std::string& type, const nlohmann::json& data = {});
};

} // namespace Wasp
