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

Object_ptr SemanticAnalyzer::visit(Constructor& constructor)
{
    Object_ptr target_type = unwrap_type_alias(visit(constructor.construtable));

    std::vector<ClassType_ptr> classes_to_check;

    if (auto generic = try_unwrap_ptr<TemplateParameterType_ptr>(target_type))
    {
        Object_ptr constraint = generic->constraint_type;

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
    }
    else if (auto cls = try_unwrap_ptr<ClassType_ptr>(target_type))
    {
        classes_to_check.push_back(cls);
    }

    Doctor::get().assert(
        !classes_to_check.empty(),
        WaspStage::Semantics,
        "Classes not found for constructor target"
    );

    ObjectVector argument_types = visit(constructor.values);

    for (const auto& class_type : classes_to_check)
    {
        Doctor::get().assert(
            argument_types.size() == class_type->fields.size(),
            WaspStage::Semantics,
            "Constructor Arguments Count Mismatch for class '" + class_type->name +
                "'."
        );

        // for (size_t i = 0; i < class_type->fields.size(); ++i)
        // {
        //     Object_ptr expected = class_type->get_member(class_type->fields[i]);
        //     Object_ptr provided = argument_types[i];

        //     Doctor::get().assert(
        //         type_system->assignable(current_scope, expected, provided),
        //         WaspStage::Semantics,
        //         "Constructor Argument Type Mismatch for field '" +
        //             class_type->fields[i] + "' in class '" + class_type->name +
        //             "'."
        //     );
        // }
    }

    return target_type;
}

} // namespace Wasp
