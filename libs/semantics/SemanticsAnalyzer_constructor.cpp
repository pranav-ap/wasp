#include "AST.h"
#include "Doctor.h"
#include "Expression.h"
#include "SemanticsAnalyzer.h"
#include "Type.h"

#include <cstddef>
#include <string>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

Type_ptr SemanticsAnalyzer::visit(Constructor& expr)
{
    Type_ptr target_type = visit(expr.constructible);
    target_type = target_type->unwrap_alias();

    TypeVector argument_types = visit(expr.arguments);

    // Box(5) - Class constructor
    if (target_type->is<ClassType_ptr>())
    {
        auto cls = target_type->as<ClassType_ptr>();

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
