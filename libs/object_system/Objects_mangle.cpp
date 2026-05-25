#include "Doctor.h"
#include "Objects.h"
#include <algorithm>
#include <cstddef>
#include <memory>
#include <string>
#include <variant>
#include <vector>

#define MAKE_OBJECT_VARIANT(x) std::make_shared<Object>(x)

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

std::string stringify_object(Object_ptr value)
{
    Doctor::get()
        .fatal_if_nullptr(value, WaspStage::VM, "Attempted to stringify a null object pointer");

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
                return "literal type: " + stringify_object(lit.value);
            },

            // Composite Types
            [](const VariantType&) -> std::string
            {
                return "variant type";
            },

            // User Defined Types
            [](const ModuleType_ptr& mod) -> std::string
            {
                return "module type: " + mod->name;
            },
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
            [](const TemplateParameterType_ptr& gen) -> std::string
            {
                return "generic type: " + gen->name;
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
                    res += stringify_object(obj->values[i]);
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
                    res += stringify_object(obj->values[i]);
                    if (i < obj->values.size() - 1)
                    {
                        res += ", ";
                    }
                }
                return res + ")";
            },
            [](const std::shared_ptr<SetObject>& obj) -> std::string
            {
                std::string res = "{";
                auto it = obj->values.begin();
                while (it != obj->values.end())
                {
                    res += stringify_object(*it);
                    if (++it != obj->values.end())
                    {
                        res += ", ";
                    }
                }
                return res + "}";
            },
            [](const std::shared_ptr<MapObject>& obj) -> std::string
            {
                std::string res = "{";
                auto it = obj->pairs.begin();
                while (it != obj->pairs.end())
                {
                    res += stringify_object(it->first) + ": " + stringify_object(it->second);
                    if (++it != obj->pairs.end())
                    {
                        res += ", ";
                    }
                }
                return res + "}";
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
            [](const std::shared_ptr<ClassBlueprintObject>&) -> std::string
            {
                return "<class blueprint>";
            },

            // Overload Groups
            [](const std::shared_ptr<ObjectOverloadList>&) -> std::string
            {
                return "<overloaded objects>";
            },

            // Instances
            [](const std::shared_ptr<ClassInstanceObject>&) -> std::string
            {
                return "<instance object>";
            },

            // Action Objects
            [](const std::shared_ptr<ErrorObject>& obj) -> std::string
            {
                return "error: " + obj->message;
            },

            // Fallback for anything missed
            [](const auto&) -> std::string
            {
                return "<Unknown Object>";
            }
        },
        value->value
    );
}

std::string mangle_object(const ObjectVector& values)
{
    std::string result;

    for (const auto& value : values)
    {
        result += mangle_object(value);
    }

    return result;
}

std::string mangle_object(Object_ptr value)
{
    Doctor::get()
        .fatal_if_nullptr(value, WaspStage::VM, "Attempted to mangle a null object pointer");

    return std::visit(
        overloaded{
            [](const std::monostate&) -> std::string
            {
                return "_";
            },

            [](const AnyType&) -> std::string
            {
                return "A";
            },
            [](const Signature_ptr&) -> std::string
            {
                return "S";
            },

            [](const NoneObject&) -> std::string
            {
                return "On";
            },
            [](const IntObject& obj) -> std::string
            {
                return "Oi" + std::to_string(obj.value);
            },
            [](const FloatObject& obj) -> std::string
            {
                return "Of" + std::to_string(obj.value);
            },
            [](const StringObject& obj) -> std::string
            {
                return "Os" + obj.value;
            },
            [](const BooleanObject& obj) -> std::string
            {
                return obj.value ? "Ob1" : "Ob0";
            },

            [](const LiteralType& lit) -> std::string
            {
                return std::visit(
                    overloaded{
                        [](const IntObject&) -> std::string
                        {
                            return "li";
                        },
                        [](const FloatObject&) -> std::string
                        {
                            return "lf";
                        },
                        [](const StringObject&) -> std::string
                        {
                            return "ls";
                        },
                        [](const BooleanObject&) -> std::string
                        {
                            return "lb";
                        },
                        [](const auto&) -> std::string
                        {
                            return "lu";
                        } // Unknown literal
                    },
                    lit.value->value
                );
            },
            [](const VariantType&) -> std::string
            {
                return "v";
            },

            [](const ModuleType_ptr& mod) -> std::string
            {
                return "M" + mod->name;
            },
            [](const ClassType_ptr& cls) -> std::string
            {
                return "C" + cls->name;
            },
            [](const TraitType_ptr& trt) -> std::string
            {
                return "T" + trt->name;
            },
            [](const EnumType_ptr& enum_type) -> std::string
            {
                return "E" + enum_type->name;
            },
            [](const TypeAlias_ptr& alias) -> std::string
            {
                return "a" + alias->name;
            },
            [](const TemplateParameterType_ptr& gen) -> std::string
            {
                return "G" + gen->name;
            },

            [](const auto&) -> std::string
            {
                return "U";
            }
        },
        value->value
    );
}

std::string get_canonical_trait_name(const ObjectVector& traits)
{
    std::vector<std::string> names;

    for (const auto& trait_obj : traits)
    {
        Doctor::get().assert(
            trait_obj->is<TraitType_ptr>(),
            WaspStage::Semantics,
            "Expected trait object in get_canonical_trait_name"
        );

        auto trait = trait_obj->as<TraitType_ptr>();
        names.push_back(trait->name);
    }

    if (names.empty())
    {
        return "";
    }

    std::sort(names.begin(), names.end());

    std::string canonical_name = names[0];
    for (size_t i = 1; i < names.size(); ++i)
    {
        canonical_name += "&" + names[i];
    }

    return canonical_name;
}

} // namespace Wasp
