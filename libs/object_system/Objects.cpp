#include "Objects.h"
#include "Doctor.h"

#include <cstddef>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#define MAKE_OBJECT_VARIANT(x) std::make_shared<Object>(x)
#define MAKE_SHARED_OBJECT_VARIANT(Type, ...)                                                      \
    std::make_shared<Object>(std::make_shared<Type>(__VA_ARGS__))
#define VOID std::make_shared<Object>(std::make_shared<ReturnObject>())
#define THROW(message) return std::make_shared<Object>(std::make_shared<ErrorObject>(message));
#define THROW_IF(condition, message)                                                               \
    if (condition)                                                                                 \
    {                                                                                              \
        return std::make_shared<Object>(std::make_shared<ErrorObject>(message));                   \
    }

namespace Wasp
{

// ============================================================================
// Membered Composite Object
// ============================================================================

Object_ptr MemberedCompositeObject::get_member(int member_id) const
{
    Doctor::get().assert(
        member_id >= 0 && member_id < static_cast<int>(members.size()),
        WaspStage::VM,
        "Member index out of bounds!"
    );

    return members[member_id];
}

void MemberedCompositeObject::set_member(int member_id, Object_ptr value)
{
    Doctor::get().assert(
        member_id >= 0 && member_id < static_cast<int>(members.size()),
        WaspStage::VM,
        "Member index out of bounds!"
    );

    members[member_id] = std::move(value);
}

int MemberedCompositeObject::get_member_count() const
{
    return members.size();
}

// ============================================================================
// Membered Composite Type
// ============================================================================

bool MemberedCompositeType::contains_member(const std::string& member_name) const
{
    return members.contains(member_name);
}

Object_ptr MemberedCompositeType::get_member(const std::string& member_name) const
{
    Doctor::get().assert(
        contains_member(member_name),
        WaspStage::Semantics,
        "Type Member '" + member_name + "' not found!"
    );

    return members.at(member_name);
}

void MemberedCompositeType::set_member(const std::string& member_name, Object_ptr value)
{
    members[member_name] = std::move(value);
}

Object_ptr MemberedCompositeType::get_member(int member_id) const
{
    Doctor::get().assert(
        member_id >= 0 && member_id < static_cast<int>(members.size()),
        WaspStage::Semantics,
        "Type Member index out of bounds!"
    );
    auto it = members.begin();
    std::advance(it, member_id);
    return it->second;
}

void MemberedCompositeType::set_member(int member_id, Object_ptr value)
{
    Doctor::get().assert(
        member_id >= 0 && member_id < static_cast<int>(members.size()),
        WaspStage::Semantics,
        "Type Member index out of bounds!"
    );
    auto it = members.begin();
    std::advance(it, member_id);
    it->second = std::move(value);
}

int MemberedCompositeType::get_member_index(const std::string& member_name) const
{
    Doctor::get().assert(
        contains_member(member_name),
        WaspStage::Semantics,
        "Member '" + member_name + "' not found!"
    );

    auto it = members.find(member_name);
    return static_cast<int>(std::distance(members.begin(), it));
}

StringVector ContainerType::get_instance_variables_declaration_order() const
{
    StringVector instance_vars;

    for (const auto& name : declaration_order)
    {
        if (shared_members.contains(name))
            continue;

        Object_ptr type_obj = members.at(name);

        if (type_obj->is<std::shared_ptr<MyMethodType>>() ||
            type_obj->is<std::shared_ptr<OurMethodType>>())
        {
            continue;
        }

        instance_vars.push_back(name);
    }

    return instance_vars;
}

StringVector ContainerType::get_class_variables_declaration_order() const
{
    StringVector class_vars;

    for (const auto& name : declaration_order)
    {
        if (!shared_members.contains(name))
            continue;

        Object_ptr type_obj = members.at(name);

        if (type_obj->is<std::shared_ptr<MyMethodType>>() ||
            type_obj->is<std::shared_ptr<OurMethodType>>())
        {
            continue;
        }

        class_vars.push_back(name);
    }

    return class_vars;
}

// ============================================================================
// StringObject
// ============================================================================

Object_ptr StringObject::get_iter()
{
    ObjectVector vec = to_vector(value);
    return MAKE_SHARED_OBJECT_VARIANT(IteratorObject, vec);
}

// ============================================================================
// IteratorObject
// ============================================================================

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

// ============================================================================
// ListObject
// ============================================================================

ListObject::ListObject()
{
}

ListObject::ListObject(ObjectVector values)
{
    for (const auto& value : values)
    {
        Doctor::get().fatal_if_nullptr(value, WaspStage::VM);
        Doctor::get().assert(
            !value->is<std::monostate>(),
            WaspStage::VM,
            "Cannot initialize a List with monostate"
        );

        this->values.push_back(value);
    }
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
    THROW_IF(index < 0 || static_cast<size_t>(index) >= values.size(), "List index out of range");

    return values[index];
}

Object_ptr ListObject::set(Object_ptr index_object, Object_ptr value)
{
    Doctor::get().fatal_if_nullptr(index_object, WaspStage::VM);
    Doctor::get().fatal_if_nullptr(value, WaspStage::VM);
    THROW_IF(!index_object->is<IntObject>(), "List indices must be integers");
    THROW_IF(value->is<std::monostate>(), "Cannot set a List element to monostate");
    int index = index_object->as<IntObject>().value;
    THROW_IF(index < 0 || static_cast<size_t>(index) >= values.size(), "List index out of range");

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

int ListObject::get_length()
{
    return values.size();
}

Object_ptr ListObject::get_iter()
{
    ObjectVector vec(values.begin(), values.end());
    return MAKE_SHARED_OBJECT_VARIANT(IteratorObject, std::move(vec));
}

// ============================================================================
// MapObject
// ============================================================================

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

    return MAKE_SHARED_OBJECT_VARIANT(TupleObject, ObjectVector{it->first, it->second});
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
        vec.push_back(MAKE_SHARED_OBJECT_VARIANT(TupleObject, ObjectVector{key, value}));
    }
    return MAKE_SHARED_OBJECT_VARIANT(IteratorObject, vec);
}

// ============================================================================
// TupleObject
// ============================================================================

Object_ptr TupleObject::get(Object_ptr index_object)
{
    Doctor::get().fatal_if_nullptr(index_object, WaspStage::VM);

    THROW_IF(!index_object->is<IntObject>(), "Tuple indices must be integers");
    int index = index_object->as<IntObject>().value;
    THROW_IF(index < 0 || static_cast<size_t>(index) >= values.size(), "Tuple index out of range");

    return values[index];
}

Object_ptr TupleObject::set(Object_ptr index_object, Object_ptr value)
{
    Doctor::get().fatal_if_nullptr(index_object, WaspStage::VM);
    Doctor::get().fatal_if_nullptr(value, WaspStage::VM);

    THROW_IF(!index_object->is<IntObject>(), "Tuple indices must be integers");
    THROW_IF(value->is<std::monostate>(), "Cannot set a Tuple element to monostate");

    int index = index_object->as<IntObject>().value;
    THROW_IF(index < 0 || static_cast<size_t>(index) >= values.size(), "Tuple index out of range");

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

int TupleObject::get_length()
{
    return values.size();
}

// ============================================================================
// SetObject
// ============================================================================

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

int SetObject::get_length()
{
    return values.size();
}

// ============================================================================
// VariantObject
// ============================================================================

bool VariantObject::has_value()
{
    return value != nullptr && !value->is<std::monostate>();
}

} // namespace Wasp
