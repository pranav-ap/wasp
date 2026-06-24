#include "AST.h"
#include "Doctor.h"
#include "Expression.h"
#include "SemanticsAnalyzer.h"
#include "Type.h"

#include <cstddef>
#include <string>
#include <vector>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

namespace
{
void handle_generic_constructor(
    GenericType_ptr template_parameter_type,
    const TypeVector& argument_types
)
{
    Type_ptr constraint = template_parameter_type->constraint_type->unwrap_alias();

    std::vector<ClassType_ptr> classes_to_check;

    if (constraint->is<VariantType_ptr>())
    {
        VariantType_ptr variant_type = constraint->as<VariantType_ptr>();

        for (const Type_ptr& variant : variant_type->types)
        {
            Type_ptr resolved = variant->unwrap_alias();

            if (resolved->is<ClassType_ptr>())
            {
                classes_to_check.push_back(resolved->as<ClassType_ptr>());
            }
        }
    }
    else if (constraint->is<ClassType_ptr>())
    {
        classes_to_check.push_back(constraint->as<ClassType_ptr>());
    }

    Doctor::semantics().check(
        !classes_to_check.empty(),
        "Classes not found for template parameter constructor target: " +
            template_parameter_type->name
    );

    for (const auto& class_type : classes_to_check)
    {
        Doctor::semantics().check(
            argument_types.size() == class_type->fields->ordered_keys.size(),
            "Constructor arguments count mismatch for class: " + class_type->name +
                ". Expected " +
                std::to_string(class_type->fields->ordered_keys.size()) + ", got " +
                std::to_string(argument_types.size()) + "."
        );
    }
}
} // namespace

Type_ptr SemanticsAnalyzer::visit(Constructor& expr)
{
    Type_ptr target_type = visit(expr.constructible);
    target_type = target_type->unwrap_alias();

    TypeVector argument_types = visit(expr.arguments);

    // T(val) - Generic parameter constructor
    if (target_type->is<GenericType_ptr>())
    {
        GenericType_ptr generic = target_type->as<GenericType_ptr>();
        handle_generic_constructor(generic, argument_types);
        return target_type;
    }

    // Box(5) - Class constructor
    if (target_type->is<ClassType_ptr>())
    {
        ClassType_ptr cls = target_type->as<ClassType_ptr>();

        Doctor::semantics().check(
            argument_types.size() == cls->fields->ordered_keys.size(),
            "Constructor Arguments Count Mismatch for class '" + cls->name +
                "'. Expected " + std::to_string(cls->fields->ordered_keys.size()) +
                ", got " + std::to_string(argument_types.size()) + "."
        );

        for (size_t i = 0; i < argument_types.size(); ++i)
        {
            const std::string& field_name = cls->fields->ordered_keys[i];

            Doctor::semantics().check(
                cls->fields->contains(field_name),
                "Field '" + field_name + "' not found in class '" + cls->name + "'."
            );

            Type_ptr expected_type = cls->fields->get_type(field_name);

            bool is_assignable = type_system->assignable(
                current_scope,
                expected_type,
                argument_types[i]
            );

            Doctor::semantics().check(
                is_assignable,
                "Type mismatch in constructor arguments"
            );
        }

        return target_type;
    }

    Doctor::semantics().fatal(
        "Invalid constructor target: '" + target_type->to_string() +
        "' is not a constructible type."
    );
}

} // namespace Wasp
