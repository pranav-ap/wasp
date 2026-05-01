#include "Doctor.h"
#include "Objects.h"
#include "SymbolScope.h"
#include "TypeChecker.h"

#include <algorithm>
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

std::shared_ptr<Signature> TypeChecker::get_function_signature(Object_ptr type_obj) const
{
    if (!type_obj)
    {
        return nullptr;
    }

    if (auto p = type_obj->try_as<std::shared_ptr<FunctionType>>())
    {
        return *p;
    }

    if (auto p = type_obj->try_as<std::shared_ptr<MethodType>>())
    {
        return *p;
    }

    if (auto p = type_obj->try_as<std::shared_ptr<FunctionTemplateType>>())
    {
        return (*p)->signature;
    }

    return nullptr;
}

Object_ptr TypeChecker::spread_type(Object_ptr type)
{
    if (!type)
        return nullptr;

    return std::visit(
        ::overloaded{
            [&](ListType const& t)
            {
                return t.element_type;
            },
            [&](TupleType const& t)
            {
                return make_object(VariantType(t.element_types));
            },
            [&](MapType const& t)
            {
                return make_object(TupleType({t.key_type, t.value_type}));
            },
            [&](const auto&) -> Object_ptr
            {
                Doctor::get().fatal(WaspStage::Semantics, "Cannot spread a non-iterable type");
                return pool->get_none_type();
            }
        },
        type->value
    );
}

Object_ptr TypeChecker::extract_iterable_element_type(
    SymbolScope_ptr scope,
    const Object_ptr type
) const
{
    if (!type)
        return make_object(AnyType());

    if (type->is<VariantType>())
    {
        auto& variant = type->as<VariantType>();
        ObjectVector extracted_elements;
        for (const auto& t : variant.types)
            extracted_elements.push_back(extract_iterable_element_type(scope, t));

        ObjectVector unique_elements = remove_duplicates(scope, extracted_elements);
        return unique_elements.size() == 1 ? unique_elements[0]
                                           : make_object(VariantType(unique_elements));
    }

    return std::visit(
        ::overloaded{
            [](ListType const& t) -> Object_ptr
            {
                return t.element_type;
            },
            [](SetType const& t) -> Object_ptr
            {
                return t.element_type;
            },
            [](MapType const& t) -> Object_ptr
            {
                return make_object(TupleType({t.key_type, t.value_type}));
            },
            [&](TupleType const& t) -> Object_ptr
            {
                ObjectVector unique = remove_duplicates(scope, t.element_types);
                return unique.size() == 1 ? unique[0] : make_object(VariantType(unique));
            },
            [&](StringType const&) -> Object_ptr
            {
                return pool->get_string_type();
            },
            [&](const auto&) -> Object_ptr
            {
                return pool->get_any_type();
            }
        },
        type->value
    );
}

bool TypeChecker::is_int_type(Object_ptr obj) const
{
    return obj->is<IntType>() || obj->is<IntLiteralType>();
}

bool TypeChecker::is_float_type(Object_ptr obj) const
{
    return obj->is<FloatType>() || obj->is<FloatLiteralType>();
}

bool TypeChecker::is_number_type(Object_ptr obj) const
{
    return is_int_type(obj) || is_float_type(obj);
}

bool TypeChecker::is_string_type(Object_ptr obj) const
{
    return obj->is<StringType>() || obj->is<StringLiteralType>();
}

bool TypeChecker::is_boolean_type(Object_ptr obj) const
{
    return obj->is<BooleanType>() || obj->is<BooleanLiteralType>();
}

bool TypeChecker::is_none_type(const Object_ptr type) const
{
    return type && holds_alternative<NoneType>(type->value);
}

bool TypeChecker::is_condition_type(SymbolScope_ptr scope, const Object_ptr condition_type) const
{
    if (!condition_type)
        return false;

    return std::visit(
        ::overloaded{
            [](BooleanType const&)
            {
                return true;
            },
            [](BooleanLiteralType const&)
            {
                return true;
            },
            [](StringType const&)
            {
                return true;
            },
            [](StringLiteralType const&)
            {
                return true;
            },
            [](ListType const&)
            {
                return true;
            },
            [](TupleType const&)
            {
                return true;
            },
            [](SetType const&)
            {
                return true;
            },
            [](MapType const&)
            {
                return true;
            },
            [&](VariantType const& t)
            {
                return std::all_of(
                    t.types.begin(),
                    t.types.end(),
                    [&](Object_ptr o)
                    {
                        return is_condition_type(scope, o);
                    }
                );
            },
            [](const auto&)
            {
                return false;
            }
        },
        condition_type->value
    );
}

bool TypeChecker::is_spreadable_type(SymbolScope_ptr scope, const Object_ptr candidate_type) const
{
    if (!candidate_type)
        return false;
    return std::visit(
        ::overloaded{
            [&](ListType const&)
            {
                return true;
            },
            [&](TupleType const&)
            {
                return true;
            },
            [&](MapType const&)
            {
                return true;
            },
            [&](VariantType const& t)
            {
                return std::all_of(
                    t.types.begin(),
                    t.types.end(),
                    [&](Object_ptr o)
                    {
                        return is_spreadable_type(scope, o);
                    }
                );
            },
            [](const auto&)
            {
                return false;
            }
        },
        candidate_type->value
    );
}

bool TypeChecker::is_iterable_type(SymbolScope_ptr scope, const Object_ptr candidate_type) const
{
    return is_string_type(candidate_type) || is_spreadable_type(scope, candidate_type);
}

bool TypeChecker::is_key_type(SymbolScope_ptr scope, const Object_ptr key_type) const
{
    return is_int_type(key_type) || is_float_type(key_type) || is_string_type(key_type) ||
           is_boolean_type(key_type);
}

void TypeChecker::expect_boolean_type(const Object_ptr type) const
{
    Doctor::get().assert(is_boolean_type(type), WaspStage::Semantics, "Expected a boolean type");
}

void TypeChecker::expect_number_type(const Object_ptr type) const
{
    Doctor::get().assert(is_number_type(type), WaspStage::Semantics, "Expected a number type");
}

void TypeChecker::expect_int_type(const Object_ptr type) const
{
    Doctor::get().assert(is_int_type(type), WaspStage::Semantics, "Expected a integer type");
}

void TypeChecker::expect_float_type(const Object_ptr type) const
{
    Doctor::get().assert(is_float_type(type), WaspStage::Semantics, "Expected a float type");
}

void TypeChecker::expect_string_type(const Object_ptr type) const
{
    Doctor::get().assert(is_string_type(type), WaspStage::Semantics, "Expected a string type");
}

void TypeChecker::expect_none_type(const Object_ptr type) const
{
    Doctor::get().assert(is_none_type(type), WaspStage::Semantics, "Expected a None type");
}

void TypeChecker::expect_condition_type(SymbolScope_ptr scope, const Object_ptr type) const
{
    Doctor::get().assert(
        is_condition_type(scope, type),
        WaspStage::Semantics,
        "Expected a valid condition type (boolean)"
    );
}

void TypeChecker::expect_spreadable_type(SymbolScope_ptr scope, const Object_ptr type) const
{
    Doctor::get().assert(
        is_spreadable_type(scope, type),
        WaspStage::Semantics,
        "Expected a spreadable collection type"
    );
}

void TypeChecker::expect_iterable_type(SymbolScope_ptr scope, const Object_ptr type) const
{
    Doctor::get()
        .assert(is_iterable_type(scope, type), WaspStage::Semantics, "Expected an iterable type");
}

void TypeChecker::expect_key_type(SymbolScope_ptr scope, const Object_ptr type) const
{
    Doctor::get().assert(
        is_key_type(scope, type),
        WaspStage::Semantics,
        "Type cannot be used as a Dictionary/Map key"
    );
}

bool TypeChecker::any_eq(SymbolScope_ptr scope, const ObjectVector& vec, const Object_ptr x) const
{
    return std::any_of(
        vec.begin(),
        vec.end(),
        [&](const auto& item)
        {
            return equal(scope, item, x);
        }
    );
}

ObjectVector TypeChecker::remove_duplicates(SymbolScope_ptr scope, const ObjectVector& vec) const
{
    ObjectVector unique_types;
    unique_types.reserve(vec.size());

    for (const auto& item : vec)
    {
        if (!any_eq(scope, unique_types, item))
        {
            unique_types.push_back(item);
        }
    }
    return unique_types;
}

} // namespace Wasp
