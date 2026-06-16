#include "Doctor.h"
#include "SymbolScope.h"
#include "Type.h"
#include "TypeSystem.h"
#include <algorithm>
#include <memory>

namespace Wasp
{

Type_ptr TypeSystem::unify(SymbolScope_ptr scope, const TypeVector& types)
{
    Doctor::semantics().assert(
        !types.empty(),
        "Cannot unify an empty set of types"
    );

    TypeVector unique_types = remove_duplicates(scope, types);

    if (unique_types.size() == 1)
    {
        return unique_types[0];
    }

    return make_type(std::make_shared<VariantType>(unique_types));
}

TypeVector TypeSystem::remove_duplicates(
    SymbolScope_ptr scope,
    const TypeVector& types
) const
{
    TypeVector unique_types;
    unique_types.reserve(types.size());

    for (const auto& item : types)
    {
        bool is_any_equal = std::any_of(
            unique_types.begin(),
            unique_types.end(),
            [&](const auto& i)
            {
                return equal(scope, i, item);
            }
        );

        if (!is_any_equal)
        {
            unique_types.push_back(item);
        }
    }

    return unique_types;
}

} // namespace Wasp
