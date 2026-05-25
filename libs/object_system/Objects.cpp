#include "Objects.h"
#include "Doctor.h"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#define MAKE_OBJECT_VARIANT(x) std::make_shared<Object>(x)
#define MAKE_SHARED_OBJECT_VARIANT(Type, ...)                                  \
    std::make_shared<Object>(std::make_shared<Type>(__VA_ARGS__))

namespace Wasp
{

Object_ptr StringObject::get_iter()
{
    ObjectVector vec;
    vec.reserve(value.size());

    for (char ch : value)
    {
        vec.push_back(MAKE_OBJECT_VARIANT(StringObject(std::string(1, ch))));
    }

    return MAKE_SHARED_OBJECT_VARIANT(IteratorObject, vec);
}

bool IteratorObject::has_next() const
{
    return index < vec.size();
}

std::optional<Object_ptr> IteratorObject::get_next()
{
    if (has_next())
    {
        return std::make_optional(vec[index++]);
    }
    return std::nullopt;
}

void IteratorObject::reset_iter()
{
    index = 0;
}

void ListObject::append(Object_ptr value)
{
    Doctor::get().fatal_if_nullptr(value, WaspStage::VM);
    Doctor::get().assert(
        value->is<std::monostate>(),
        WaspStage::VM,
        "Cannot append monostate to a List"
    );
    values.push_back(value);
}

void ListObject::prepend(Object_ptr value)
{
    Doctor::get().fatal_if_nullptr(value, WaspStage::VM);
    Doctor::get().assert(
        !value->is<std::monostate>(),
        WaspStage::VM,
        "Cannot prepend monostate to a List"
    );

    values.insert(values.begin(), value);
}

Object_ptr ListObject::pop_back()
{
    Doctor::get().assert(
        !values.empty(),
        WaspStage::VM,
        "Cannot pop from an empty list"
    );

    Object_ptr last = values.back();
    values.pop_back();
    return last;
}

Object_ptr ListObject::pop_front()
{
    Doctor::get().assert(
        !values.empty(),
        WaspStage::VM,
        "Cannot pop from an empty list"
    );

    Object_ptr first = values.front();
    values.erase(values.begin());
    return first;
}

Object_ptr ListObject::get(Object_ptr index_object)
{
    Doctor::get().fatal_if_nullptr(index_object, WaspStage::VM);
    Doctor::get().assert(
        index_object->is<IntObject>(),
        WaspStage::VM,
        "List indices must be integers"
    );

    int index = index_object->as<IntObject>().value;
    Doctor::get().assert(
        index >= 0 && static_cast<size_t>(index) < values.size(),
        WaspStage::VM,
        "List index out of range"
    );

    return values[index];
}

void ListObject::set(Object_ptr index_object, Object_ptr value)
{
    Doctor::get().fatal_if_nullptr(index_object, WaspStage::VM);
    Doctor::get().fatal_if_nullptr(value, WaspStage::VM);

    Doctor::get().assert(
        index_object->is<IntObject>(),
        WaspStage::VM,
        "List indices must be integers"
    );

    Doctor::get().assert(
        !value->is<std::monostate>(),
        WaspStage::VM,
        "Cannot set a List element to monostate"
    );

    int index = index_object->as<IntObject>().value;

    Doctor::get().assert(
        index >= 0 && static_cast<size_t>(index) < values.size(),
        WaspStage::VM,
        "List index out of range"
    );

    values[index] = value;
}

void ListObject::clear()
{
    values.clear();
}

bool ListObject::is_empty()
{
    return values.empty();
}

Object_ptr ListObject::get_iter()
{
    ObjectVector vec(values.begin(), values.end());
    return MAKE_SHARED_OBJECT_VARIANT(IteratorObject, std::move(vec));
}

void MapObject::insert(Object_ptr key, Object_ptr value)
{
    Doctor::get().fatal_if_nullptr(key, WaspStage::VM);
    Doctor::get().fatal_if_nullptr(value, WaspStage::VM);

    Doctor::get().assert(
        !key->is<std::monostate>(),
        WaspStage::VM,
        "Cannot use monostate as a Map key"
    );

    Doctor::get().assert(
        !value->is<std::monostate>(),
        WaspStage::VM,
        "Cannot use monostate as a Map value"
    );

    const auto& [it, inserted] = pairs.insert({key, value});

    Doctor::get()
        .assert(inserted, WaspStage::VM, "Key already exists in the Map");
}

void MapObject::set(Object_ptr key, Object_ptr value)
{
    Doctor::get().fatal_if_nullptr(key, WaspStage::VM);
    Doctor::get().fatal_if_nullptr(value, WaspStage::VM);

    Doctor::get().assert(
        !key->is<std::monostate>(),
        WaspStage::VM,
        "Cannot use monostate as a Map key"
    );

    Doctor::get().assert(
        !value->is<std::monostate>(),
        WaspStage::VM,
        "Cannot use monostate as a Map value"
    );

    auto it = pairs.find(key);

    Doctor::get().assert(
        it != pairs.end(),
        WaspStage::VM,
        "Key does not exist in the Map"
    );

    it->second = value;
}

int MapObject::get_size()
{
    return pairs.size();
}

Object_ptr MapObject::get_pair(Object_ptr key)
{
    Doctor::get().fatal_if_nullptr(key, WaspStage::VM);

    Doctor::get().assert(
        !key->is<std::monostate>(),
        WaspStage::VM,
        "Cannot use monostate as a Map key"
    );

    auto it = pairs.find(key);

    Doctor::get().assert(
        it != pairs.end(),
        WaspStage::VM,
        "Key does not exist in the Map"
    );

    return MAKE_SHARED_OBJECT_VARIANT(
        TupleObject,
        ObjectVector{it->first, it->second}
    );
}

Object_ptr MapObject::get(Object_ptr key)
{
    Doctor::get().fatal_if_nullptr(key, WaspStage::VM);

    Doctor::get().assert(
        !key->is<std::monostate>(),
        WaspStage::VM,
        "Cannot use monostate as a Map key"
    );

    auto it = pairs.find(key);
    Doctor::get().assert(
        it != pairs.end(),
        WaspStage::VM,
        "Key does not exist in the Map"
    );

    return it->second;
}

Object_ptr MapObject::get_iter()
{
    ObjectVector vec;
    for (const auto& [key, value] : pairs)
    {
        vec.push_back(
            MAKE_SHARED_OBJECT_VARIANT(TupleObject, ObjectVector{key, value})
        );
    }
    return MAKE_SHARED_OBJECT_VARIANT(IteratorObject, vec);
}

Object_ptr TupleObject::get(Object_ptr index_object)
{
    Doctor::get().fatal_if_nullptr(index_object, WaspStage::VM);

    Doctor::get().assert(
        index_object->is<IntObject>(),
        WaspStage::VM,
        "Tuple indices must be integers"
    );

    int index = index_object->as<IntObject>().value;

    Doctor::get().assert(
        index >= 0 && static_cast<size_t>(index) < values.size(),
        WaspStage::VM,
        "Tuple index out of range"
    );

    return values[index];
}

void TupleObject::set(Object_ptr index_object, Object_ptr value)
{
    Doctor::get().fatal_if_nullptr(index_object, WaspStage::VM);
    Doctor::get().fatal_if_nullptr(value, WaspStage::VM);

    Doctor::get().assert(
        index_object->is<IntObject>(),
        WaspStage::VM,
        "Tuple indices must be integers"
    );

    Doctor::get().assert(
        !value->is<std::monostate>(),
        WaspStage::VM,
        "Cannot set a Tuple element to monostate"
    );

    int index = index_object->as<IntObject>().value;

    Doctor::get().assert(
        index >= 0 && static_cast<size_t>(index) < values.size(),
        WaspStage::VM,
        "Tuple index out of range"
    );

    values[index] = value;
}

void TupleObject::set(ObjectVector values)
{
    for (const auto& value : values)
    {
        Doctor::get().fatal_if_nullptr(value, WaspStage::VM);
        Doctor::get().assert(
            !value->is<std::monostate>(),
            WaspStage::VM,
            "Cannot set a Tuple element to monostate"
        );
    }

    this->values = values;
}

ObjectVector SetObject::get()
{
    return values;
}

void SetObject::set(ObjectVector values)
{
    for (const auto& value : values)
    {
        Doctor::get().fatal_if_nullptr(value, WaspStage::VM);
        Doctor::get().assert(
            !value->is<std::monostate>(),
            WaspStage::VM,
            "Cannot set a Set element to monostate"
        );
    }

    this->values = values;
}

Object_ptr SetObject::get_iter()
{
    ObjectVector vec(values.begin(), values.end());
    return MAKE_SHARED_OBJECT_VARIANT(IteratorObject, std::move(vec));
}

bool VariantObject::has_value()
{
    return value != nullptr && !value->is<std::monostate>();
}

// ============================================================================
// Object Method Implementations
// ============================================================================

bool Object::is_type_object() const
{
    return is<VariantType>() || is<Signature_ptr>() || is<ModuleType_ptr>() ||
           is<ClassType_ptr>() || is<TraitType_ptr>() || is<EnumType_ptr>() ||
           is<TypeAlias_ptr>() || is<TemplateParameterType_ptr>() ||
           is<LiteralType>();
}

bool Object::is_runtime_value() const
{
    // A runtime value is anything that isn't a type, an empty state,
    // an overload group, or a control-flow action object.
    return !is_type_object() && !is<std::monostate>() &&
           !is<std::shared_ptr<ObjectOverloadList>>();
}

bool Object::is_callable() const
{
    return is<std::shared_ptr<FunctionBlueprintObject>>() ||
           is<std::shared_ptr<FunctionRuntimeObject>>() ||
           is<std::shared_ptr<NativeFunctionObject>>() || is<Signature_ptr>();
}

IterableAbstractObject* Object::as_iterable()
{
    if (auto* str = try_as<StringObject>())
    {
        return str;
    }

    if (auto* list_ptr = try_as<std::shared_ptr<ListObject>>())
    {
        return list_ptr->get();
    }

    if (auto* set_ptr = try_as<std::shared_ptr<SetObject>>())
    {
        return set_ptr->get();
    }

    if (auto* map_ptr = try_as<std::shared_ptr<MapObject>>())
    {
        return map_ptr->get();
    }

    return nullptr;
}

int EnumType::get_value(const std::vector<std::string>& path) const
{
    std::stringstream ss;

    for (size_t i = 0; i < path.size(); ++i)
    {
        ss << path[i] << (i == path.size() - 1 ? "" : ".");
    }

    std::string search_path = ss.str();
    auto it = std::find(members.begin(), members.end(), search_path);

    if (it != members.end())
    {
        return static_cast<int>(std::distance(members.begin(), it));
    }

    Doctor::get().fatal(
        WaspStage::Semantics,
        "Enum '" + name + "' does not contain member '" + search_path + "'."
    );
}

} // namespace Wasp
