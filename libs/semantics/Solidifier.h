#pragma once

#include "AST.h"
#include "Statement.h"
#include "Type.h"

#include <map>
#include <string>
#include <unordered_map>

namespace Wasp
{

class Solidifier
{
public:
    static Solidifier& get()
    {
        static Solidifier instance;
        return instance;
    }

    // Public API – all take semantic Type maps
    Statement_ptr solidify(
        const FunctionDefinition& func,
        const std::map<std::string, Type_ptr>& substitution_map
    );

    Statement_ptr solidify(
        const MethodDefinition& method,
        const std::map<std::string, Type_ptr>& substitution_map
    );

    Statement_ptr solidify(
        const ClassDefinition& cls,
        const std::map<std::string, Type_ptr>& substitution_map
    );

    Statement_ptr solidify(
        const TraitDefinition& trait,
        const std::map<std::string, Type_ptr>& substitution_map
    );

    Statement_ptr solidify(
        const RecordDefinition& record,
        const std::map<std::string, Type_ptr>& substitution_map
    );

    Statement_ptr solidify(
        const OperatorDefinition& op,
        const std::map<std::string, Type_ptr>& substitution_map
    );

    // Generic entry for any statement
    Statement_ptr solidify(
        const Statement_ptr& stmt,
        const std::map<std::string, Type_ptr>& substitution_map
    );

    StatementVector solidify(
        const StatementVector& statements,
        const std::map<std::string, Type_ptr>& substitution_map
    );

    Block solidify_block(const Block& block, const std::map<std::string, Type_ptr>& substitution_map);

    Expression_ptr solidify(
        const Expression_ptr& expr,
        const std::map<std::string, Type_ptr>& substitution_map
    );

    ExpressionVector solidify(
        const ExpressionVector& expressions,
        const std::map<std::string, Type_ptr>& substitution_map
    );

    TypeNode_ptr solidify(
        const TypeNode_ptr& type_node,
        const std::map<std::string, Type_ptr>& substitution_map
    );

    TypeNodeVector solidify(
        const TypeNodeVector& types,
        const std::map<std::string, Type_ptr>& substitution_map
    );

    Field solidify(const Field& field, const std::map<std::string, Type_ptr>& substitution_map);

    FieldVector solidify(const FieldVector& fields, const std::map<std::string, Type_ptr>& substitution_map);

    // Access solidified instances for code generation
    const std::unordered_map<std::string, Statement_ptr>& get_solidified() const
    {
        return solidified_instances;
    }

    void clear()
    {
        solidified_instances.clear();
    }

private:
    Solidifier() = default;

    std::unordered_map<std::string, Statement_ptr> solidified_instances;

    // --- Private helpers that work with TypeNode maps ---
    Statement_ptr solidify_node(
        const FunctionDefinition& func,
        const std::map<std::string, TypeNode_ptr>& typenode_map
    );

    Statement_ptr solidify_node(
        const MethodDefinition& method,
        const std::map<std::string, TypeNode_ptr>& typenode_map
    );

    Statement_ptr solidify_node(
        const ClassDefinition& cls,
        const std::map<std::string, TypeNode_ptr>& typenode_map
    );

    Statement_ptr solidify_node(
        const TraitDefinition& trait,
        const std::map<std::string, TypeNode_ptr>& typenode_map
    );

    Statement_ptr solidify_node(
        const RecordDefinition& record,
        const std::map<std::string, TypeNode_ptr>& typenode_map
    );

    Statement_ptr solidify_node(
        const OperatorDefinition& op,
        const std::map<std::string, TypeNode_ptr>& typenode_map
    );

    Statement_ptr solidify_node(
        const Statement_ptr& stmt,
        const std::map<std::string, TypeNode_ptr>& typenode_map
    );

    StatementVector solidify_node(
        const StatementVector& statements,
        const std::map<std::string, TypeNode_ptr>& typenode_map
    );

    Block solidify_node(const Block& block, const std::map<std::string, TypeNode_ptr>& typenode_map);

    Expression_ptr solidify_node(
        const Expression_ptr& expr,
        const std::map<std::string, TypeNode_ptr>& typenode_map
    );

    ExpressionVector solidify_node(
        const ExpressionVector& expressions,
        const std::map<std::string, TypeNode_ptr>& typenode_map
    );

    TypeNode_ptr solidify_node(
        const TypeNode_ptr& type_node,
        const std::map<std::string, TypeNode_ptr>& typenode_map
    );

    TypeNodeVector solidify_node(
        const TypeNodeVector& types,
        const std::map<std::string, TypeNode_ptr>& typenode_map
    );

    Field solidify_node(const Field& field, const std::map<std::string, TypeNode_ptr>& typenode_map);

    FieldVector solidify_node(
        const FieldVector& fields,
        const std::map<std::string, TypeNode_ptr>& typenode_map
    );

    // --- Type → TypeNode conversion helpers ---
    TypeNode_ptr type_to_typenode(Type_ptr type);
    TypeNodeVector types_to_typenodes(const TypeVector& types);
    std::map<std::string, TypeNode_ptr> make_typenode_substitutions(
        const std::map<std::string, Type_ptr>& substitutions
    );

    // --- Name mangling ---
    std::string mangle(const Type_ptr& type);
    std::string mangle(const TypeVector& types);
    std::string get_solidified_name(const std::string& base_name, const TypeVector& type_arguments);

    bool is_generic(const FieldVector& generics) const;
};

} // namespace Wasp
