#pragma once
#include "AST.h"

#include <string>

namespace Wasp
{

class ASTFactory
{
public:
    static Expression_ptr create_identifier(const std::string& name);

    static Expression_ptr create_function_call(
        const std::string& function_name,
        ExpressionVector arguments = {}
    );

    static Expression_ptr create_method_call(
        Expression_ptr target,
        const std::string& method_name,
        ExpressionVector arguments = {}
    );

    static Expression_ptr create_constructor(
        Expression_ptr construtable,
        ExpressionVector values = {}
    );

    static Expression_ptr create_field_definition(
        const std::string& field_name,
        TypeAnnotation_ptr type_node
    );
};

} // namespace Wasp
