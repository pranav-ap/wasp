#pragma once

#include "AST.h"
#include "Statement.h"
#include "Type.h"

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

    Statement_ptr visit(Statement_ptr& stmt, const std::map<std::string, Type_ptr>& substitution_map);
    Statement_ptr visit(FunctionDefinition& func, const std::map<std::string, Type_ptr>& substitution_map);
    Type_ptr substitute_type(Type_ptr type, std::map<std::string, Type_ptr>& substitutions) const;

private:
    Solidifier() = default;

    Statement_ptr visit(Statement_ptr& stmt, const std::map<std::string, TypeNode_ptr>& typenode_map);

    StatementVector visit(
        StatementVector& statements,
        const std::map<std::string, TypeNode_ptr>& typenode_map
    );

    Statement_ptr visit(FunctionDefinition& func, const std::map<std::string, TypeNode_ptr>& typenode_map);

    Block solidify(Block& block, const std::map<std::string, TypeNode_ptr>& typenode_map);

    TypeNode_ptr visit(TypeNode_ptr& type_node, const std::map<std::string, TypeNode_ptr>& typenode_map);
    TypeNodeVector visit(TypeNodeVector& types, const std::map<std::string, TypeNode_ptr>& typenode_map);

    Field visit(Field& field, const std::map<std::string, TypeNode_ptr>& typenode_map);
    FieldVector visit(FieldVector& fields, const std::map<std::string, TypeNode_ptr>& typenode_map);

    TypeNode_ptr type_to_typenode(Type_ptr type);
    TypeNodeVector types_to_typenodes(TypeVector& types);

    std::map<std::string, TypeNode_ptr> make_typenode_substitutions(
        const std::map<std::string, Type_ptr>& substitutions
    );
};

} // namespace Wasp
