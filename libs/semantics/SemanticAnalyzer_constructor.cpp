#include "AST.h"
#include "Doctor.h"
#include "Expression.h"
#include "Objects.h"
#include "SemanticAnalyzer.h"
#include "SymbolScope.h"
#include "Workspace.h"

#include <cctype>
#include <ctime>
#include <memory>
#include <string>
#include <vector>

namespace Wasp
{

Object_ptr SemanticAnalyzer::visit(Constructor& constructor)
{
    ObjectVector argument_types = visit(constructor.values);
    Object_ptr target_type = nullptr;

    if (auto* target = constructor.construtable->try_as<Identifier>())
    {
        target_type = visit(*target);
    }
    else if (auto* tc = constructor.construtable->try_as<TemplateAngular>())
    {
        target_type = visit(*tc);
    }
    else
    {
        Doctor::get().fatal(WaspStage::Semantics, "Invalid constructor target");
    }

    target_type = unwrap_type_alias(target_type);

    std::vector<ClassType_ptr> classes_to_check;
    if (auto cls = try_unwrap_ptr<ClassType_ptr>(target_type))
    {
        classes_to_check.push_back(cls);
    }
    else if (auto generic = try_unwrap_ptr<TemplateParameterType_ptr>(target_type))
    {
        Object_ptr constraint = generic->constraint_type;
        if (auto* union_type = constraint->try_as<VariantType>())
        {
            for (auto& variant : union_type->types)
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

    Doctor::get().assert(
        !classes_to_check.empty(),
        WaspStage::Semantics,
        "Constructor target must resolve to a class or a template constrained by "
        "classes."
    );

    for (auto& class_type : classes_to_check)
    {
        Doctor::get().assert(
            argument_types.size() == class_type->fields.size(),
            WaspStage::Semantics,
            "Constructor Arguments Count Mismatch for class '" + class_type->name +
                "'."
        );

        for (size_t i = 0; i < class_type->fields.size(); ++i)
        {
            Object_ptr expected = class_type->get_member(class_type->fields[i]);
            Object_ptr provided = argument_types[i];
            bool is_valid = false;

            // Handle Union arguments by checking variant compatibility
            if (auto* union_arg = provided->try_as<VariantType>())
            {
                for (auto& variant : union_arg->types)
                {
                    if (type_system->assignable(current_scope, expected, variant))
                    {
                        is_valid = true;
                        break;
                    }
                }
            }
            else
            {
                is_valid = type_system
                               ->assignable(current_scope, expected, provided);
            }

            Doctor::get().assert(
                is_valid,
                WaspStage::Semantics,
                "Constructor Argument Type Mismatch for field '" +
                    class_type->fields[i] + "' in class '" + class_type->name + "'."
            );
        }
    }

    return target_type;
}

} // namespace Wasp
