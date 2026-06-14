#include "Doctor.h"
#include "Type.h"

#include <string>
#include <variant>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

std::string Type::to_string() const
{
    Doctor::get().fatal_if_nullptr(
        this,
        WaspStage::VM,
        "Attempted to stringify a null object pointer"
    );

    return std::visit(
        overloaded{
            [](std::monostate) -> std::string
            {
                return "uninitialized";
            },

            [](AnyType_ptr) -> std::string
            {
                return "any type";
            },
            [](NoneType_ptr) -> std::string
            {
                return "none type";
            },

            [](IntType_ptr) -> std::string
            {
                return "int";
            },
            [](FloatType_ptr) -> std::string
            {
                return "float";
            },
            [](StringType_ptr) -> std::string
            {
                return "str";
            },
            [](BooleanType_ptr) -> std::string
            {
                return "bool";
            },
            [](LiteralType_ptr lit) -> std::string
            {
                return "literal type: " + lit->value->to_string();
            },

            [](ListType_ptr) -> std::string
            {
                return "list type";
            },
            [](SetType_ptr) -> std::string
            {
                return "set type";
            },
            [](TupleType_ptr) -> std::string
            {
                return "tuple type";
            },
            [](MapType_ptr) -> std::string
            {
                return "map type";
            },

            [](VariantType_ptr) -> std::string
            {
                return "variant type";
            },
            [](IntersectionType_ptr) -> std::string
            {
                return "intersection type";
            },

            [](EnumType_ptr enum_type) -> std::string
            {
                return "enum type: " + enum_type->name;
            },
            [](EnumMemberType_ptr) -> std::string
            {
                return "enum member";
            },

            [](GenericType_ptr gen) -> std::string
            {
                return "generic type: " + gen->name;
            },

            [](ModuleType_ptr module) -> std::string
            {
                return "module type: " + module->name;
            },

            [](Signature_ptr) -> std::string
            {
                return "signature type";
            },

            [](ClassType_ptr cls) -> std::string
            {
                return "class type: " + cls->name;
            },
            [](TraitType_ptr trt) -> std::string
            {
                return "trait type: " + trt->name;
            },
            [](PrimitiveType_ptr prim) -> std::string
            {
                return "primitive type: " + prim->name;
            },

            [](TypeAlias_ptr alias) -> std::string
            {
                return "type alias: " + alias->name;
            },

            [](const auto&) -> std::string
            {
                return "<Unknown Object>";
            }
        },
        this->data
    );
}

Type_ptr Type::unwrap_alias()
{
    if (is<TypeAlias_ptr>())
    {
        auto alias = as<TypeAlias_ptr>();

        if (alias->underlying_type)
        {
            return alias->underlying_type->unwrap_alias();
        }
    }

    return shared_from_this();
}

} // namespace Wasp
