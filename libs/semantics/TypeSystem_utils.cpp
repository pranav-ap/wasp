#include "Doctor.h"
// keep
#include "Type.h"
#include "TypeSystem.h"
#include <variant>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

Type_ptr TypeSystem::unpack_primitive(const Type_ptr type) const
{
    Doctor::semantics().fatal_if_nullptr(type);

    return std::visit(
        overloaded{
            [](PrimitiveType_ptr primitive) -> Type_ptr
            {
                if (primitive->name == "int")
                {
                    return make_shared_type<IntType>();
                }
                if (primitive->name == "float")
                {
                    return make_shared_type<FloatType>();
                }
                if (primitive->name == "str")
                {
                    return make_shared_type<StringType>();
                }
                if (primitive->name == "bool")
                {
                    return make_shared_type<BooleanType>();
                }

                Doctor::semantics().fatal(
                    "Unknown primitive type: " + primitive->name
                );
            },

            [&](const auto&) -> Type_ptr
            {
                return type;
            }
        },
        type->data
    );
}

} // namespace Wasp
