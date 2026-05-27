#include "ASTFactory.h"
#include "AST.h"
#include "Expression.h"
#include "Statement.h"
#include <string>
#include <vector>

namespace Wasp
{

Statement_ptr create_class_definition(
    std::string name,
    TypeAnnotationVector traits,
    StatementVector members,
    FieldDefinitionVector template_params
)
{
    auto class_def_node = make_statement(
        ClassDefinition{name, traits, members, template_params}
    );

    return class_def_node;
}

Statement_ptr create_trait_definition(
    std::string name,
    TypeAnnotationVector traits,
    StatementVector members,
    FieldDefinitionVector template_params
)
{
    auto trait_def_node = make_statement(
        TraitDefinition{name, traits, members, template_params}
    );

    return trait_def_node;
}

Statement_ptr create_function_definition(
    std::string name,
    std::vector<Field> params,
    TypeAnnotation_ptr ret,
    StatementVector body,
    bool is_pure,
    FieldDefinitionVector template_params
)
{
    auto func_def_node = make_statement(
        FunctionDefinition{name, params, ret, body, is_pure, template_params}
    );

    return func_def_node;
}

Field create_field(
    const std::string& name,
    TypeAnnotation_ptr type,
    bool is_static
)
{
    return Field(name, type, is_static);
}

Expression_ptr create_field_definition(
    const std::string& field_name,
    TypeAnnotation_ptr type_node
)
{
    auto field_def_node = make_expression(
        FieldDefinition{field_name, type_node}
    );

    return field_def_node;
}

Expression_ptr ASTFactory::create_identifier(const std::string& name)
{
    return make_expression(Identifier(name));
};

Expression_ptr ASTFactory::create_function_call(
    const std::string& function_name,
    ExpressionVector arguments
)
{
    auto function_id = create_identifier(function_name);
    auto call_node = make_expression(Call(function_id, arguments));
    return call_node;
}

Expression_ptr ASTFactory::create_method_call(
    Expression_ptr target,
    const std::string& method_name,
    ExpressionVector arguments
)
{
    auto method_id = create_identifier(method_name);
    auto member_access = make_expression(MemberAccess(target, method_id));
    auto call_node = make_expression(Call(member_access, arguments));
    return call_node;
}

Expression_ptr create_constructor(
    Expression_ptr construtable,
    ExpressionVector values = {}
)
{
    auto constructor_node = make_expression(Constructor(construtable, values));
    return constructor_node;
}

} // namespace Wasp
