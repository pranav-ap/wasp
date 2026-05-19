#include "Liar.h"
#include "AST.h"
#include "Statement.h"
#include "TypeAnnotation.h"
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace Wasp
{

StatementVector Liar::run(const StatementVector& statements)
{
    StatementVector processed_ast;

    for (const auto& stmt : statements)
    {
        LiarResult result = visit(stmt);

        if (auto* vec = std::get_if<StatementVector>(&result))
        {
            processed_ast.insert(processed_ast.end(), vec->begin(), vec->end());
        }
        else
        {
            processed_ast.push_back(std::get<Statement_ptr>(result));
        }
    }

    return processed_ast;
}

LiarResult Liar::visit(Statement_ptr stmt)
{
    if (auto* cls = stmt->try_as<ClassDefinition>())
    {
        return visit(*cls);
    }

    return stmt;
}

LiarResult Liar::visit(ClassDefinition& def)
{
    StatementVector output;
    StatementVector extracted_functions;
    StatementVector kept_members;

    for (auto& member : def.members)
    {
        if (auto* method = member->try_as<MethodDefinition>())
        {
            extracted_functions.push_back(transform_method_to_function(def.name, *method));
        }
        else
        {
            kept_members.push_back(member);
        }
    }

    def.members = kept_members;

    output.push_back(std::make_shared<Statement>(def));
    output.insert(output.end(), extracted_functions.begin(), extracted_functions.end());

    return output;
}

Statement_ptr Liar::transform_method_to_function(
    const std::string& class_name,
    MethodDefinition& method
)
{
    std::string new_name = class_name + "::" + method.name;

    std::vector<std::pair<std::string, TypeAnnotation_ptr>> new_params;

    TypeAnnotation_ptr class_type = make_type_annotation(TypeIdentifierNode(class_name));

    std::string context_name = method.is_static ? "our" : "my";
    new_params.emplace_back(context_name, class_type);

    new_params.insert(new_params.end(), method.parameters.begin(), method.parameters.end());

    auto new_func = std::make_shared<FunctionDefinition>(
        new_name,
        std::move(new_params),
        method.return_type,
        method.body,
        method.is_pure,
        std::move(method.template_params)
    );

    return make_statement<FunctionDefinition>(std::move(*new_func));
}

} // namespace Wasp
