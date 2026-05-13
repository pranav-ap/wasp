#include "Objects.h"
#include "Doctor.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>

#define MAKE_OBJECT_VARIANT(x) std::make_shared<Object>(x)
#define MAKE_SHARED_OBJECT_VARIANT(Type, ...)                                       \
    std::make_shared<Object>(std::make_shared<Type>(__VA_ARGS__))
#define VOID std::make_shared<Object>(std::make_shared<ReturnObject>())
#define THROW(message)                                                              \
    return std::make_shared<Object>(std::make_shared<ErrorObject>(message));
#define THROW_IF(condition, message)                                                \
    if (condition)                                                                  \
    {                                                                               \
        return std::make_shared<Object>(std::make_shared<ErrorObject>(message));    \
    }

namespace Wasp
{

Object_ptr StringObject::get_iter()
{
    ObjectVector vec = to_vector(value);
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

Object_ptr ListObject::append(Object_ptr value)
{
    Doctor::get().fatal_if_nullptr(value, WaspStage::VM);
    THROW_IF(value->is<std::monostate>(), "Cannot append monostate to a List");
    values.push_back(value);
    return VOID;
}

Object_ptr ListObject::prepend(Object_ptr value)
{
    Doctor::get().fatal_if_nullptr(value, WaspStage::VM);
    THROW_IF(value->is<std::monostate>(), "Cannot prepend monostate to a List");
    values.insert(values.begin(), value);
    return VOID;
}

Object_ptr ListObject::pop_back()
{
    if (values.empty())
    {
        THROW("Cannot pop from an empty list");
    }

    Object_ptr last = values.back();
    values.pop_back();
    return last;
}

Object_ptr ListObject::pop_front()
{
    if (values.empty())
    {
        THROW("Cannot pop from an empty list");
    }

    Object_ptr first = values.front();
    values.erase(values.begin());
    return first;
}

Object_ptr ListObject::get(Object_ptr index_object)
{
    Doctor::get().fatal_if_nullptr(index_object, WaspStage::VM);
    THROW_IF(!index_object->is<IntObject>(), "List indices must be integers");
    int index = index_object->as<IntObject>().value;
    THROW_IF(
        index < 0 || static_cast<size_t>(index) >= values.size(),
        "List index out of range"
    );

    return values[index];
}

Object_ptr ListObject::set(Object_ptr index_object, Object_ptr value)
{
    Doctor::get().fatal_if_nullptr(index_object, WaspStage::VM);
    Doctor::get().fatal_if_nullptr(value, WaspStage::VM);
    THROW_IF(!index_object->is<IntObject>(), "List indices must be integers");
    THROW_IF(value->is<std::monostate>(), "Cannot set a List element to monostate");
    int index = index_object->as<IntObject>().value;
    THROW_IF(
        index < 0 || static_cast<size_t>(index) >= values.size(),
        "List index out of range"
    );

    values[index] = value;
    return VOID;
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

Object_ptr MapObject::insert(Object_ptr key, Object_ptr value)
{
    Doctor::get().fatal_if_nullptr(key, WaspStage::VM);
    Doctor::get().fatal_if_nullptr(value, WaspStage::VM);
    THROW_IF(key->is<std::monostate>(), "Cannot use monostate as a Map key");
    THROW_IF(value->is<std::monostate>(), "Cannot use monostate as a Map value");

    const auto& [it, inserted] = pairs.insert({key, value});
    THROW_IF(!inserted, "Key already exists in the Map");

    return VOID;
}

Object_ptr MapObject::set(Object_ptr key, Object_ptr value)
{
    Doctor::get().fatal_if_nullptr(key, WaspStage::VM);
    Doctor::get().fatal_if_nullptr(value, WaspStage::VM);
    THROW_IF(key->is<std::monostate>(), "Cannot use monostate as a Map key");
    THROW_IF(value->is<std::monostate>(), "Cannot use monostate as a Map value");

    auto it = pairs.find(key);
    THROW_IF(it == pairs.end(), "Key does not exist in the Map");

    it->second = value;
    return VOID;
}

int MapObject::get_size()
{
    return pairs.size();
}

Object_ptr MapObject::get_pair(Object_ptr key)
{
    Doctor::get().fatal_if_nullptr(key, WaspStage::VM);
    THROW_IF(key->is<std::monostate>(), "Cannot use monostate as a Map key");

    auto it = pairs.find(key);
    THROW_IF(it == pairs.end(), "Key does not exist in the Map");

    return MAKE_SHARED_OBJECT_VARIANT(
        TupleObject,
        ObjectVector{it->first, it->second}
    );
}

Object_ptr MapObject::get(Object_ptr key)
{
    Doctor::get().fatal_if_nullptr(key, WaspStage::VM);
    THROW_IF(key->is<std::monostate>(), "Cannot use monostate as a Map key");

    auto it = pairs.find(key);
    THROW_IF(it == pairs.end(), "Key does not exist in the Map");

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

    THROW_IF(!index_object->is<IntObject>(), "Tuple indices must be integers");
    int index = index_object->as<IntObject>().value;
    THROW_IF(
        index < 0 || static_cast<size_t>(index) >= values.size(),
        "Tuple index out of range"
    );

    return values[index];
}

Object_ptr TupleObject::set(Object_ptr index_object, Object_ptr value)
{
    Doctor::get().fatal_if_nullptr(index_object, WaspStage::VM);
    Doctor::get().fatal_if_nullptr(value, WaspStage::VM);

    THROW_IF(!index_object->is<IntObject>(), "Tuple indices must be integers");
    THROW_IF(value->is<std::monostate>(), "Cannot set a Tuple element to monostate");

    int index = index_object->as<IntObject>().value;
    THROW_IF(
        index < 0 || static_cast<size_t>(index) >= values.size(),
        "Tuple index out of range"
    );

    values[index] = value;
    return VOID;
}

Object_ptr TupleObject::set(ObjectVector values)
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
    return VOID;
}

ObjectVector SetObject::get()
{
    return values;
}

Object_ptr SetObject::set(ObjectVector values)
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
    return VOID;
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
    return is<TypeType>() || is<AnyType>() || is<NoneType>() || is<IntType>() ||
           is<FloatType>() || is<StringType>() || is<BooleanType>() ||
           is<ListType>() || is<TupleType>() || is<SetType>() || is<MapType>() ||
           is<VariantType>() || is<Signature_ptr>() || is<ModuleType_ptr>() ||
           is<ClassType_ptr>() || is<TraitType_ptr>() || is<EnumType_ptr>() ||
           is<TypeAlias_ptr>() || is<TemplateParameterType_ptr>() ||
           is<LiteralType>() || is<NamedDefinitionType>();
}

bool Object::is_runtime_value() const
{
    // A runtime value is anything that isn't a type, an empty state,
    // an overload group, or a control-flow action object.
    return !is_type_object() && !is<std::monostate>() &&
           !is<std::shared_ptr<BreakObject>>() &&
           !is<std::shared_ptr<ContinueObject>>() &&
           !is<std::shared_ptr<RedoObject>>() &&
           !is<std::shared_ptr<ReturnObject>>() &&
           !is<std::shared_ptr<ErrorObject>>() &&
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

} // namespace Wasp
