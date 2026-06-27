#include "AST.h"
#include "Doctor.h"
#include "Expression.h"
#include "SemanticsAnalyzer.h"
#include "Solidifier.h"
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
    // Resolve target type (e.g., Foo, T)
    Type_ptr target_type = visit(expr.constructible);
    target_type = target_type->unwrap_alias();

    TypeVector solid_types = visit(expr.angular_nodes);
    TypeVector argument_types = visit(expr.arguments);

    if (target_type->is<ClassType_ptr>())
    {
        ClassType_ptr class_type = target_type->as<ClassType_ptr>();

        // If the class has template parameters and we have explicit arguments
        if (class_type->template_type && !class_type->template_type->empty())
        {
            Doctor::semantics().check(
                !solid_types.empty(),
                "Class template requires explicit template arguments"
            );

            const StringVector& param_names = class_type->template_type->ordered_parameter_names;

            Doctor::semantics().check(
                solid_types.size() == param_names.size(),
                "Template argument count mismatch for class '" + class_type->name + "'. Expected " +
                    std::to_string(param_names.size()) + ", got " + std::to_string(solid_types.size()) + "."
            );

            // Build substitution map: parameter name -> solid type
            TypeSubstitutionMap substitutions;
            for (size_t i = 0; i < param_names.size(); ++i)
            {
                substitutions[param_names[i]] = solid_types[i];
            }

            // Substitute the class type to produce a specialized version
            Type_ptr specialized_type = Solidifier::get().substitute_type(target_type, substitutions);
            specialized_type = specialized_type->unwrap_alias();

            Doctor::semantics().check(
                specialized_type->is<ClassType_ptr>(),
                "Substitution must yield a class type"
            );

            // Now treat it as a concrete class
            class_type = specialized_type->as<ClassType_ptr>();
            target_type = specialized_type;

            // Optionally, create a symbol for the specialized class if needed
            // (similar to function template instantiation)
            // but for constructors we may not need a new symbol if we just use the type.
        }
        else
        {
            // Non-template class must not have angular nodes
            Doctor::semantics().check(
                solid_types.empty(),
                "Non-template class does not accept template arguments"
            );
        }

        Doctor::semantics().check(
            argument_types.size() == class_type->fields->ordered_keys.size(),
            "Constructor Arguments Count Mismatch for class '" + class_type->name + "'. Expected " +
                std::to_string(class_type->fields->ordered_keys.size()) + ", got " +
                std::to_string(argument_types.size()) + "."
        );

        for (size_t i = 0; i < argument_types.size(); ++i)
        {
            const std::string& field_name = class_type->fields->ordered_keys[i];
            Type_ptr expected_type = class_type->fields->get_type(field_name);

            bool is_assignable = type_system->assignable(current_scope, expected_type, argument_types[i]);

            Doctor::semantics().check(
                is_assignable,
                "Type mismatch in constructor arguments for field '" + field_name + "'"
            );
        }

        return target_type;
    }

    // Case 2: Target is a generic type parameter (e.g., T(val) in a generic context)
    if (target_type->is<GenericType_ptr>())
    {
        GenericType_ptr generic = target_type->as<GenericType_ptr>();
        handle_generic_constructor(generic, argument_types);
        return target_type;
    }

    Doctor::semantics().fatal(target_type->to_string() + " is not a constructible type");
}

} // namespace Wasp
