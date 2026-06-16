#include "AST.h"
#include "Doctor.h"
#include "Statement.h"
#include "SymbolFactory.h"
#include "Type.h"
#include "TypeChecker.h"

#include <map>
#include <memory>
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

TemplateType_ptr TypeChecker::create_template_type(
    const FieldVector& generics
)
{
    TypeStringMap generics_map;
    StringVector ordered_names;
    bool seen_variadic_generic = false;

    for (const auto& generic : generics)
    {
        Type_ptr constraint_type = make_shared_type<GenericType>(
            generic.name
        );

        if (constraint_type->is<IntersectionType_ptr>())
        {
            auto types = constraint_type->as<IntersectionType_ptr>()
                             ->types;

            for (auto& inner_type : types)
            {
                Doctor::get().assert(
                    inner_type->is<TraitType_ptr>(),
                    WaspStage::Semantics,
                    "Only an interesection of traits is supported"
                );
            }
        }
        else if (constraint_type->is<VariantType_ptr>())
        {
            auto types = constraint_type->as<VariantType_ptr>()->types;

            for (auto& inner_type : types)
            {
                Doctor::get().assert(
                    type_system->is_primitive_type(inner_type),
                    WaspStage::Semantics,
                    "Only an union of primitives is supported"
                );
            }
        }

        Doctor::get().assert(
            !generics_map.contains(generic.name),
            WaspStage::Semantics,
            "Duplicate generic parameter name: " + generic.name
        );

        if (generic.is_variadic)
        {
            Doctor::get().assert(
                !seen_variadic_generic,
                WaspStage::Semantics,
                "Only one variadic generic parameter is allowed"
            );

            seen_variadic_generic = true;
        }

        auto generic_type = make_shared_type<GenericType>(
            generic.name,
            constraint_type,
            generic.is_variadic
        );

        generics_map[generic.name] = generic_type;
        ordered_names.push_back(generic.name);
    }

    return std::make_shared<TemplateType>(
        std::move(generics_map),
        std::move(ordered_names)
    );
}

void TypeChecker::define_template_type(TemplateType_ptr template_type)
{
    if (template_type->empty())
    {
        return;
    }

    auto ordered_generics = template_type->get_ordered_generics();

    for (const auto& [name, generic_type] : ordered_generics)
    {
        auto symbol = SymbolFactory::create_type(name, generic_type);
        current_scope->define(symbol);
    }
}

} // namespace Wasp
