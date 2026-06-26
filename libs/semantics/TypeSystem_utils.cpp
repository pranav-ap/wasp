#include "Doctor.h"
// keep
#include "Type.h"
#include "TypeSystem.h"
#include <string>
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

std::string TypeSystem::mangle(const Type_ptr& type)
{
    return std::visit(
        overloaded{
            [](IntType_ptr) -> std::string
            {
                return "i";
            },
            [](FloatType_ptr) -> std::string
            {
                return "d";
            },
            [](StringType_ptr) -> std::string
            {
                return "s";
            },
            [](BooleanType_ptr) -> std::string
            {
                return "b";
            },
            [](NoneType_ptr) -> std::string
            {
                return "n";
            },

            [](PrimitiveType_ptr primitive) -> std::string
            {
                if (primitive->name == "int")
                {
                    return "i";
                }
                if (primitive->name == "float")
                {
                    return "d";
                }
                if (primitive->name == "str")
                {
                    return "s";
                }
                if (primitive->name == "bool")
                {
                    return "b";
                }
                return primitive->name;
            },

            [&](ListType_ptr list) -> std::string
            {
                return "L" + mangle(list->element_type);
            },

            [&](TupleType_ptr tuple) -> std::string
            {
                std::string result = "T";
                for (const auto& elem : tuple->element_types)
                {
                    result += mangle(elem);
                }
                return result;
            },

            [&](MapType_ptr map) -> std::string
            {
                return "M" + mangle(map->key_type) + mangle(map->value_type);
            },

            [&](FunctionType_ptr func) -> std::string
            {
                std::string result = "F";

                for (const auto& param : func->parameter_types)
                {
                    result += mangle(param);
                }

                result += "_" + mangle(func->return_type);

                return result;
            },

            [&](MethodType_ptr method) -> std::string
            {
                std::string result = "M";

                for (const auto& param : method->parameter_types)
                {
                    result += mangle(param);
                }

                result += "_" + mangle(method->return_type);

                return result;
            },

            [](ClassType_ptr cls) -> std::string
            {
                return "C" + cls->name;
            },
            [](TraitType_ptr trait) -> std::string
            {
                return "T" + trait->name;
            },

            [](GenericType_ptr generic) -> std::string
            {
                return "G" + generic->name;
            },

            [](EnumType_ptr enum_type) -> std::string
            {
                return "E" + enum_type->name;
            },

            [](ModuleType_ptr module) -> std::string
            {
                return "mod" + module->name;
            },

            [](const auto&) -> std::string
            {
                Doctor::semantics().fatal("Cannot mangle name for this type");
            }
        },
        type->data
    );
}

std::string TypeSystem::mangle(const TypeVector& generic_types)
{
    std::string result = "";

    for (const auto& type : generic_types)
    {
        result += mangle(type);
    }

    return result;
}

} // namespace Wasp
