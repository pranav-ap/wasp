#include "Objects.h"
#include "Doctor.h"

#include <cstddef>
#include <memory>
#include <string>
#include <variant>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

std::string Object::to_string() const
{
    Doctor::get().fatal_if_nullptr(
        value,
        WaspStage::VM,
        "Attempted to stringify a null object pointer"
    );

    return std::visit(
        overloaded{
            [](const std::monostate&) -> std::string
            {
                return "uninitialized";
            },

            // Base Types
            [](const AnyType&) -> std::string
            {
                return "any type";
            },
            [](const Signature_ptr&) -> std::string
            {
                return "signature type";
            },

            // Scalar Objects
            [](const NoneObject&) -> std::string
            {
                return "none";
            },
            [](const IntObject& obj) -> std::string
            {
                return std::to_string(obj.value);
            },
            [](const FloatObject& obj) -> std::string
            {
                return std::to_string(obj.value);
            },
            [](const StringObject& obj) -> std::string
            {
                return "\"" + obj.value + "\"";
            },
            [](const BooleanObject& obj) -> std::string
            {
                return obj.value ? "true" : "false";
            },

            // Unified Literal Type
            [](const LiteralType& lit) -> std::string
            {
                return "literal type: " + lit.value.get()->to_string();
            },

            // Composite Types
            [](const VariantType&) -> std::string
            {
                return "variant type";
            },

            // User Defined Types

            [](const ClassType_ptr& cls) -> std::string
            {
                return "class type: " + cls->name;
            },
            [](const TraitType_ptr& trt) -> std::string
            {
                return "trait type: " + trt->name;
            },
            [](const EnumType_ptr& enum_type) -> std::string
            {
                return "enum type: " + enum_type->name;
            },
            [](const TypeAlias_ptr& alias) -> std::string
            {
                return "type alias: " + alias->name;
            },
            [](const GenericType& gen) -> std::string
            {
                return "generic type: " + gen.name;
            },

            // Composite Objects
            [](const std::shared_ptr<IteratorObject>&) -> std::string
            {
                return "<iterator>";
            },
            [](const std::shared_ptr<ListObject>& obj) -> std::string
            {
                std::string res = "[";
                for (size_t i = 0; i < obj->values.size(); ++i)
                {
                    res += obj->values[i]->to_string();
                    if (i < obj->values.size() - 1)
                    {
                        res += ", ";
                    }
                }
                return res + "]";
            },
            [](const std::shared_ptr<TupleObject>& obj) -> std::string
            {
                std::string res = "(";
                for (size_t i = 0; i < obj->values.size(); ++i)
                {
                    res += obj->values[i]->to_string();
                    if (i < obj->values.size() - 1)
                    {
                        res += ", ";
                    }
                }
                return res + ")";
            },
            [](const std::shared_ptr<SetObject>& obj) -> std::string
            {
                return "set";
            },
            [](const std::shared_ptr<MapObject>& obj) -> std::string
            {
                return "map";
            },
            [](const std::shared_ptr<VariantObject>&) -> std::string
            {
                return "<variant>";
            },

            // Callables and Modules
            [](const std::shared_ptr<FunctionBlueprintObject>& func)
                -> std::string
            {
                return "<Static Function " + func->name + ">";
            },
            [](const std::shared_ptr<FunctionRuntimeObject>& func)
                -> std::string
            {
                return "<Runtime function " + func->blueprint->name + ">";
            },
            [](const std::shared_ptr<NativeFunctionObject>& func) -> std::string
            {
                return "<Native function " + func->name + ">";
            },
            [](const std::shared_ptr<ModuleObject>& mod) -> std::string
            {
                return "<module " + mod->name + ">";
            },
            // Fallback for anything missed
            [](const auto&) -> std::string
            {
                return "<Unknown Object>";
            }
        },
        this->value
    );
}

// ============================================================================
// Utils
// ============================================================================

Object_ptr Object::unwrap_type_alias()
{
    if (this->is<TypeAlias_ptr>())
    {
        return this->as<TypeAlias_ptr>()->underlying_type->unwrap_type_alias();
    }

    return this->shared_from_this();
}

Object_ptr Object::unwrap_completely()
{
    if (this->is<TypeAlias_ptr>())
    {
        return this->as<TypeAlias_ptr>()->underlying_type->unwrap_completely();
    }

    if (this->is<GenericType>())
    {
        auto& generic = this->as<GenericType>();

        if (generic.constraint_type)
        {
            return generic.constraint_type->unwrap_completely();
        }
        else
        {
            return this->shared_from_this();
        }
    }

    return this->shared_from_this();
}

bool Object::is_type_object() const
{
    return std::visit(
        overloaded{
            [](AnyType_ptr) -> bool
            {
                return true;
            },
            [](NoneType_ptr) -> bool
            {
                return true;
            },
            [](IntType_ptr) -> bool
            {
                return true;
            },
            [](FloatType_ptr) -> bool
            {
                return true;
            },
            [](StringType_ptr) -> bool
            {
                return true;
            },
            [](BooleanType_ptr) -> bool
            {
                return true;
            },
            [](LiteralType_ptr) -> bool
            {
                return true;
            },
            [](ListType_ptr) -> bool
            {
                return true;
            },
            [](SetType_ptr) -> bool
            {
                return true;
            },
            [](TupleType_ptr) -> bool
            {
                return true;
            },
            [](MapType_ptr) -> bool
            {
                return true;
            },
            [](VariantType_ptr) -> bool
            {
                return true;
            },
            [](IntersectionType_ptr) -> bool
            {
                return true;
            },
            [](GenericType_ptr) -> bool
            {
                return true;
            },
            [](EnumMemberType_ptr) -> bool
            {
                return true;
            },
            [](Signature_ptr) -> bool
            {
                return true;
            },
            [](ClassType_ptr) -> bool
            {
                return true;
            },
            [](TraitType_ptr) -> bool
            {
                return true;
            },
            [](EnumType_ptr) -> bool
            {
                return true;
            },
            [](TypeAlias_ptr) -> bool
            {
                return true;
            },
            [](const auto&) -> bool
            {
                return false;
            }
        },
        value
    );
}

bool Object::is_runtime_object() const
{
    return !is_type_object();
}

} // namespace Wasp
