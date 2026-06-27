#include "AST.h"
#include "Doctor.h"
#include "Expression.h"
#include "SemanticsAnalyzer.h"
#include "Solidifier.h"
#include "Type.h"

#include <cstddef>
#include <string>
#include <utility>
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

    for (const ClassType_ptr& class_type : classes_to_check)
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
    Type_ptr constructible_type = visit(expr.constructible);
    constructible_type = constructible_type->unwrap_alias();

    TypeVector solid_types = visit(expr.angular_nodes);
    TypeVector argument_types = visit(expr.arguments);

    if (constructible_type->is<ClassType_ptr>())
    {
        ClassType_ptr class_type = constructible_type->as<ClassType_ptr>();

        if (class_type->template_type->empty())
        {
            validate_solid_constructor(class_type, solid_types, argument_types);
            return constructible_type;
        }

        auto [solid_type, substitutions] = validate_constructor_template(
            class_type,
            solid_types,
            argument_types
        );

        return solid_type;
    }

    if (constructible_type->is<GenericType_ptr>())
    {
        GenericType_ptr generic_type = constructible_type->as<GenericType_ptr>();
        handle_generic_constructor(generic_type, argument_types);
        return constructible_type;
    }

    Doctor::semantics().fatal(constructible_type->to_string() + " is not a constructible type");
}

void SemanticsAnalyzer::validate_solid_constructor(
    ClassType_ptr class_type,
    TypeVector solid_types,
    TypeVector argument_types
)
{
    Doctor::semantics().check(solid_types.empty(), "Non-template class does not accept template arguments");

    Doctor::semantics().check(
        argument_types.size() == class_type->fields->ordered_keys.size(),
        "Constructor Arguments Count Mismatch for class '" + class_type->name + "'. Expected " +
            std::to_string(class_type->fields->ordered_keys.size()) + ", got " +
            std::to_string(argument_types.size()) + "."
    );

    for (size_t i = 0; i < argument_types.size(); ++i)
    {
        const std::string& field_name = class_type->fields->ordered_keys[i];
        const Type_ptr expected_type = class_type->fields->get_type(field_name);

        bool is_assignable = type_system->assignable(current_scope, expected_type, argument_types[i]);

        Doctor::semantics().check(
            is_assignable,
            "Type mismatch in constructor arguments for field '" + field_name + "'"
        );
    }
}

std::pair<Type_ptr, TypeSubstitutionMap> SemanticsAnalyzer::validate_constructor_template(
    ClassType_ptr class_type,
    TypeVector solid_types,
    TypeVector argument_types
)
{
    Doctor::semantics().check(!solid_types.empty(), "Class template requires explicit template arguments");

    const StringVector& generic_names = class_type->template_type->ordered_parameter_names;

    Doctor::semantics().check(
        solid_types.size() == generic_names.size(),
        "Template argument count mismatch for class '" + class_type->name + "'. Expected " +
            std::to_string(generic_names.size()) + ", got " + std::to_string(solid_types.size()) + "."
    );

    // Build substitution map: generic name -> solid type
    TypeSubstitutionMap substitutions;
    for (size_t i = 0; i < generic_names.size(); ++i)
    {
        substitutions[generic_names[i]] = solid_types[i];
    }

    Type_ptr solid_type = Solidifier::get().substitute_type(class_type, substitutions);

    validate_solid_constructor(solid_type->as<ClassType_ptr>(), solid_types, argument_types);

    return {solid_type, substitutions};
}

} // namespace Wasp
