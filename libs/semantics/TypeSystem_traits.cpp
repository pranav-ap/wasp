#include "Type.h"
#include "TypeSystem.h"

#include <string>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{
bool TypeSystem::implements_trait(
    Type_ptr patient,
    const std::string& trait_name
) const
{
    if (patient->is<GenericType_ptr>())
    {
        GenericType_ptr generic_type = patient->as<GenericType_ptr>();

        if (generic_type->constraint_type)
        {
            return implements_trait(generic_type->constraint_type, trait_name);
        }
        else
        {
            return false;
        }
    }
    else if (patient->is<ClassType_ptr>())
    {
        ClassType_ptr oops_type = patient->as<ClassType_ptr>();
        return implements_trait(oops_type, trait_name);
    }
    else if (patient->is<TraitType_ptr>())
    {
        TraitType_ptr oops_type = patient->as<TraitType_ptr>();
        return implements_trait(oops_type, trait_name);
    }
    else if (patient->is<PrimitiveType_ptr>())
    {
        PrimitiveType_ptr oops_type = patient->as<PrimitiveType_ptr>();
        return implements_trait(oops_type, trait_name);
    }
    else if (patient->is<IntersectionType_ptr>())
    {
        IntersectionType_ptr intersection_type = patient->as<IntersectionType_ptr>();

        for (const auto& type : intersection_type->types)
        {
            if (implements_trait(type, trait_name))
            {
                return true;
            }
        }

        return false;
    }
    else if (patient->is<VariantType_ptr>())
    {
        VariantType_ptr variant_type = patient->as<VariantType_ptr>();

        for (const auto& type : variant_type->types)
        {
            if (!implements_trait(type, trait_name))
            {
                return false;
            }
        }

        return true;
    }

    return false;
}

bool TypeSystem::implements_trait(
    OopsType_ptr patient,
    const std::string& trait_name
) const
{
    for (const auto& trait_obj : patient->traits)
    {
        if (trait_obj->is<TraitType_ptr>())
        {
            if (trait_obj->as<TraitType_ptr>()->name == trait_name)
            {
                return true;
            }
        }
    }

    return false;
}

} // namespace Wasp
