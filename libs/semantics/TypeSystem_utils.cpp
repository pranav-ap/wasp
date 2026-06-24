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

std::string TypeSystem::mangle_name(const Type_ptr& type) const
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
                return "L" + mangle_name(list->element_type);
            },

            [&](TupleType_ptr tuple) -> std::string
            {
                std::string result = "T";
                for (const auto& elem : tuple->element_types)
                {
                    result += mangle_name(elem);
                }
                return result;
            },

            [&](MapType_ptr map) -> std::string
            {
                return "M" + mangle_name(map->key_type) +
                       mangle_name(map->value_type);
            },

            [&](FunctionType_ptr func) -> std::string
            {
                std::string result = "F";
                if (func->signature)
                {
                    for (const auto& param : func->signature->parameter_types)
                    {
                        result += mangle_name(param);
                    }
                    result += "_" + mangle_name(func->signature->return_type);
                }
                return result;
            },

            [&](MethodType_ptr method) -> std::string
            {
                std::string result = "M";
                if (method->signature)
                {
                    for (const auto& param : method->signature->parameter_types)
                    {
                        result += mangle_name(param);
                    }
                    result += "_" + mangle_name(method->signature->return_type);
                }
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

std::string TypeSystem::mangle_name(const TypeVector& generic_types) const
{
    std::string result = "";

    for (const auto& type : generic_types)
    {
        result += mangle_name(type);
    }

    return result;
}

} // namespace Wasp
