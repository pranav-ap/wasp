#pragma once
#include "AST.h"
#include "Statement.h"

#include <string>
#include <vector>

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

    static Statement_ptr create_class_definition(
        std::string name,
        TypeAnnotationVector traits,
        StatementVector members,
        FieldDefinitionVector template_params
    );

    static Statement_ptr create_trait_definition(
        std::string name,
        TypeAnnotationVector traits,
        StatementVector members,
        FieldDefinitionVector template_params
    );

    static Statement_ptr create_function_definition(
        std::string name,
        std::vector<Field> params,
        TypeAnnotation_ptr ret,
        StatementVector body,
        bool is_pure = false,
        FieldDefinitionVector template_params = {}
    );

    static Field create_field(const std::string& name, TypeAnnotation_ptr type);
};

} // namespace Wasp
