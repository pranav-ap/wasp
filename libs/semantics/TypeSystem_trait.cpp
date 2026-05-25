#include "Doctor.h"
#include "Objects.h"
#include "SymbolScope.h"
#include "TypeSystem.h"

#include <memory>
#include <string>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

bool TypeSystem::implements_trait(
    SymbolScope_ptr scope,
    const Object_ptr patient,
    const std::string& trait_name
) const
{
    Doctor::get().assert(
        patient->is<ClassType_ptr>() || patient->is<TraitType_ptr>(),
        WaspStage::Semantics,
        "Expected a class or trait type to check for trait implementation"
    );

    OopsType_ptr patient_type = patient->is<ClassType_ptr>()
                                    ? std::static_pointer_cast<OopsType>(
                                          patient->as<ClassType_ptr>()
                                      )
                                    : std::static_pointer_cast<OopsType>(
                                          patient->as<TraitType_ptr>()
                                      );

    auto traits = patient_type->get_traits();

    for (const auto& trait : traits)
    {
        Doctor::get().assert(
            trait->is<TraitType_ptr>(),
            WaspStage::Semantics,
            "Expected traits to be of type TraitType"
        );

        if (trait->as<TraitType_ptr>()->name == trait_name)
        {
            return true;
        }
    }

    return false;
}

bool TypeSystem::implements_trait(
    SymbolScope_ptr scope,
    const Object_ptr patient,
    const int trait_type_id
) const
{
    Doctor::get().assert(
        patient->is<ClassType_ptr>() || patient->is<TraitType_ptr>(),
        WaspStage::Semantics,
        "Expected a class or trait type to check for trait implementation"
    );

    OopsType_ptr patient_type = patient->is<ClassType_ptr>()
                                    ? std::static_pointer_cast<OopsType>(
                                          patient->as<ClassType_ptr>()
                                      )
                                    : std::static_pointer_cast<OopsType>(
                                          patient->as<TraitType_ptr>()
                                      );

    auto traits = patient_type->get_traits();

    for (const auto& trait : traits)
    {
        Doctor::get().assert(
            trait->is<TraitType_ptr>(),
            WaspStage::Semantics,
            "Expected traits to be of type TraitType"
        );

        if (trait->as<TraitType_ptr>()->type_id == trait_type_id)
        {
            return true;
        }
    }

    return false;
}

bool TypeSystem::implements_trait(
    SymbolScope_ptr scope,
    const Object_ptr patient,
    const Object_ptr trait
) const
{
    Doctor::get().assert(
        trait->is<TraitType_ptr>(),
        WaspStage::Semantics,
        "Expected an object of type TraitType to check for trait implementation"
    );

    return implements_trait(scope, patient, trait->as<TraitType_ptr>()->type_id);
}

} // namespace Wasp
