#pragma once

#include "AST.h"
#include "Statement.h"

#include <map>
#include <string>

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

    Solidifier(const Solidifier&) = delete;
    Solidifier& operator=(const Solidifier&) = delete;
    Solidifier(Solidifier&&) = delete;
    Solidifier& operator=(Solidifier&&) = delete;

    // ========================================================================
    // Entry points
    // ========================================================================

    Statement_ptr solidify(
        const FunctionDefinition& func,
        const TypeNodeVector& type_arguments
    );

    Statement_ptr solidify(
        const FunctionDefinition& func,
        const std::map<std::string, TypeNode_ptr>& substitution_map
    );

    Statement_ptr solidify(
        const MethodDefinition& method,
        const FieldVector& generics,
        const TypeNodeVector& type_arguments
    );

    Statement_ptr solidify(
        const MethodDefinition& method,
        const std::map<std::string, TypeNode_ptr>& substitution_map
    );

    Statement_ptr solidify(
        const RecordDefinition& cls,
        const TypeNodeVector& type_arguments
    );

    Statement_ptr solidify(
        const RecordDefinition& cls,
        const std::map<std::string, TypeNode_ptr>& substitution_map
    );

    Statement_ptr solidify(
        const ClassDefinition& cls,
        const TypeNodeVector& type_arguments
    );

    Statement_ptr solidify(
        const ClassDefinition& cls,
        const std::map<std::string, TypeNode_ptr>& substitution_map
    );

    Statement_ptr solidify(
        const TraitDefinition& trait,
        const TypeNodeVector& type_arguments
    );

    Statement_ptr solidify(
        const TraitDefinition& trait,
        const std::map<std::string, TypeNode_ptr>& substitution_map
    );

    Statement_ptr solidify(
        const Statement_ptr& stmt,
        const std::map<std::string, TypeNode_ptr>& substitution_map
    );

    StatementVector solidify(
        const StatementVector& statements,
        const std::map<std::string, TypeNode_ptr>& substitution_map
    );

    Statement_ptr solidify(
        const Block& block,
        const std::map<std::string, TypeNode_ptr>& substitution_map
    );

    Block solidify_block(
        const Block& block,
        const std::map<std::string, TypeNode_ptr>& substitution_map
    );

    Expression_ptr solidify(
        const Expression_ptr& expr,
        const std::map<std::string, TypeNode_ptr>& substitution_map
    );

    ExpressionVector solidify(
        const ExpressionVector& expr,
        const std::map<std::string, TypeNode_ptr>& substitution_map
    );

    TypeNode_ptr solidify(
        const TypeNode_ptr& type_node,
        const std::map<std::string, TypeNode_ptr>& substitution_map
    );

    TypeNodeVector solidify(
        const TypeNodeVector& types,
        const std::map<std::string, TypeNode_ptr>& substitution_map
    );

    Field solidify(
        const Field& field,
        const std::map<std::string, TypeNode_ptr>& substitution_map
    );

    FieldVector solidify(
        const FieldVector& fields,
        const std::map<std::string, TypeNode_ptr>& substitution_map
    );

private:
    Solidifier() = default;

    // ========================================================================
    // Utilities
    // ========================================================================

    std::map<std::string, TypeNode_ptr> build_substitution_map(
        const FieldVector& generics,
        const TypeNodeVector& type_arguments
    );

    bool is_generic(const FieldVector& generics) const;

    std::string mangle(const TypeNodeVector& args);

    std::string mangle(const TypeNode_ptr& type);

    std::string get_solid_name(
        const std::string& base_name,
        const TypeNodeVector& type_arguments
    );
};

} // namespace Wasp
