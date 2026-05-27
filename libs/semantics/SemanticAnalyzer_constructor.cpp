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
    GenericType_ptr template_parameter_type,
    const ObjectVector& argument_types
)
{
    Object_ptr constraint = template_parameter_type->constraint_type;
    std::vector<ClassType_ptr> classes_to_check;

    // Check if constraint is a VariantType (union of types)
    if (constraint->is<VariantType_ptr>())
    {
        auto variant_type = constraint->as<VariantType_ptr>();
        for (const auto& variant : variant_type->types)
        {
            if (variant->is<ClassType_ptr>())
            {
                classes_to_check.push_back(variant->as<ClassType_ptr>());
            }
        }
    }
    // Check if constraint is a single ClassType
    else if (constraint->is<ClassType_ptr>())
    {
        classes_to_check.push_back(constraint->as<ClassType_ptr>());
    }

    Doctor::get().assert(
        !classes_to_check.empty(),
        WaspStage::Semantics,
        "Classes not found for template parameter constructor target: " +
            template_parameter_type->name
    );

    for (const auto& class_type : classes_to_check)
    {
        Doctor::get().assert(
            argument_types.size() == class_type->fields.size(),
            WaspStage::Semantics,
            "Constructor arguments count mismatch for class: " +
                class_type->name + ". Expected " +
                std::to_string(class_type->fields.size()) + ", got " +
                std::to_string(argument_types.size()) + "."
        );
    }
}

Object_ptr SemanticAnalyzer::visit(Constructor& constructor)
{
    Object_ptr target_type = visit(constructor.construtable);
    target_type = target_type->unwrap_type_alias();

    ObjectVector argument_types = visit(constructor.values);

    // T(val) - Generic parameter constructor
    if (target_type->is<GenericType_ptr>())
    {
        auto generic = target_type->as<GenericType_ptr>();
        analyze_template_parameter_constructor(generic, argument_types);
        return target_type;
    }

    // Box(5) - Class constructor
    if (target_type->is<ClassType_ptr>())
    {
        auto cls = target_type->as<ClassType_ptr>();

        Doctor::get().assert(
            argument_types.size() == cls->fields.size(),
            WaspStage::Semantics,
            "Constructor Arguments Count Mismatch for class '" + cls->name +
                "'. Expected " + std::to_string(cls->fields.size()) + ", got " +
                std::to_string(argument_types.size()) + "."
        );

        for (size_t i = 0; i < argument_types.size(); ++i)
        {
            const std::string& field_name = cls->fields[i];

            Doctor::get().assert(
                cls->member_types.contains(field_name),
                WaspStage::Semantics,
                "Field '" + field_name + "' not found in class '" + cls->name +
                    "'."
            );

            Object_ptr expected_type = cls->member_types.at(field_name);

            Doctor::get().assert(
                type_system->assignable(
                    current_scope,
                    expected_type,
                    argument_types[i]
                ),
                WaspStage::Semantics,
                "Type mismatch in constructor for field '" + field_name +
                    "' of class '" + cls->name + "'. Expected '" +
                    expected_type->to_string() + "', got '" +
                    argument_types[i]->to_string() + "'."
            );
        }

        return target_type;
    }

    // Handle Record construction (desugared form)
    if (target_type->is<RecordObject_ptr>())
    {
        // Records are runtime values, not types
        Doctor::get().fatal(
            WaspStage::Semantics,
            "Cannot construct a record value directly. Use class constructor."
        );
    }

    Doctor::get().fatal(
        WaspStage::Semantics,
        "Invalid constructor target: '" + target_type->to_string() +
            "' is not a constructible type."
    );
}

} // namespace Wasp
