#include "AST.h"
#include "Doctor.h"
#include "Expression.h"
#include "Objects.h"
#include "SemanticAnalyzer.h"

#include <cctype>
#include <ctime>
#include <string>
#include <vector>

namespace Wasp
{

void SemanticAnalyzer::analyze_template_parameter_constructor(
    TemplateParameterType_ptr template_parameter_type,
    const ObjectVector& argument_types
)
{
    Object_ptr constraint = template_parameter_type->constraint_type;
    std::vector<ClassType_ptr> classes_to_check;

    if (auto* union_type = constraint->try_as<VariantType>())
    {
        for (const auto& variant : union_type->types)
        {
            if (auto cls = try_unwrap_ptr<ClassType_ptr>(variant))
            {
                classes_to_check.push_back(cls);
            }
        }
    }
    else if (auto cls = try_unwrap_ptr<ClassType_ptr>(constraint))
    {
        classes_to_check.push_back(cls);
    }

    Doctor::get().assert(
        !classes_to_check.empty(),
        WaspStage::Semantics,
        "Classes not found for template parameter constructor target : " +
            template_parameter_type->name
    );

    for (const auto& class_type : classes_to_check)
    {
        Doctor::get().assert(
            argument_types.size() == class_type->fields.size(),
            WaspStage::Semantics,
            "Constructor Arguments Count Mismatch for class :" + class_type->name
        );
    }
}

Object_ptr SemanticAnalyzer::visit(Constructor& constructor)
{
    Object_ptr target_type = unwrap_type_alias(visit(constructor.construtable));
    ObjectVector argument_types = visit(constructor.values);

    // T(val)
    if (auto generic = try_unwrap_ptr<TemplateParameterType_ptr>(target_type))
    {
        analyze_template_parameter_constructor(generic, argument_types);
        return target_type;
    }

    // Box(5)
    if (auto cls = try_unwrap_ptr<ClassType_ptr>(target_type))
    {
        Doctor::get().assert(
            argument_types.size() == cls->fields.size(),
            WaspStage::Semantics,
            "Constructor Arguments Count Mismatch for class '" + cls->name + "'."
        );

        return target_type;
    }

    Doctor::get().fatal(WaspStage::Semantics, "Invalid constructor target.");
}

} // namespace Wasp
