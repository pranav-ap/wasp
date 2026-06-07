#include "ASTFactory.h"
#include "AST.h"
#include "Expression.h"
#include "Statement.h"
#include <string>
#include <utility>
#include <vector>

namespace Wasp
{

Statement_ptr ASTFactory::create_class_definition(
    std::string name,
    TypeAnnotationVector traits,
    StatementVector members,
    FieldDefinitionVector template_params
)
{
    return make_statement(
        ClassDefinition{
            std::move(name),
            std::move(traits),
            std::move(members),
            std::move(template_params)
        }
    );
}

Statement_ptr ASTFactory::create_trait_definition(
    std::string name,
    TypeAnnotationVector traits,
    StatementVector members,
    FieldDefinitionVector template_params
)
{
    return make_statement(
        TraitDefinition{
            std::move(name),
            std::move(traits),
            std::move(members),
            std::move(template_params)
        }
    );
}

Statement_ptr ASTFactory::create_function_definition(
    std::string name,
    std::vector<Field> params,
    TypeAnnotation_ptr ret,
    StatementVector body,
    bool is_pure,
    FieldDefinitionVector template_params
)
{
    return make_statement(
        FunctionDefinition{
            std::move(name),
            std::move(params),
            std::move(ret),
            std::move(body),
            is_pure,
            std::move(template_params)
        }
    );
}

Field ASTFactory::create_field(const std::string& name, TypeAnnotation_ptr type)
{
    return Field(name, std::move(type));
}

Expression_ptr ASTFactory::create_identifier(const std::string& name)
{
    return make_expression(Identifier(name));
}

Expression_ptr ASTFactory::create_function_call(
    const std::string& function_name,
    ExpressionVector arguments
)
{
    auto function_id = create_identifier(function_name);
    return make_expression(Call(function_id, std::move(arguments)));
}

Expression_ptr ASTFactory::create_method_call(
    Expression_ptr target,
    const std::string& method_name,
    ExpressionVector arguments
)
{
    auto method_id = create_identifier(method_name);
    auto member_access = make_expression(MemberAccess(target, method_id));
    return make_expression(Call(member_access, std::move(arguments)));
}

Expression_ptr ASTFactory::create_constructor(
    Expression_ptr construtable,
    ExpressionVector values
)
{
    return make_expression(Constructor(construtable, std::move(values)));
}

} // namespace Wasp
