#include "Doctor.h"
#include "Objects.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>

namespace Wasp
{

Object_ptr StringObject::get_iter()
{
    ObjectVector vec;
    vec.reserve(value.size());

    for (char ch : value)
    {
        vec.push_back(make_object(StringObject(std::string(1, ch))));
    }

    return make_object(std::make_shared<IteratorObject>(vec));
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
    return make_object(std::make_shared<IteratorObject>(vec));
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
            make_object(std::make_shared<TupleObject>(ObjectVector{key, value}))
        );
    }

    return make_object(std::make_shared<IteratorObject>(vec));
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
    return make_object(std::make_shared<IteratorObject>(vec));
}

bool VariantObject::has_value()
{
    return value != nullptr && !value->is<std::monostate>();
}

// -----------------------------------------------------------------------------
// Function Overloads Object
// ------------------------------------------------------------------------------

void Pocket::add_overload(Object_ptr overload)
{
    overloads.push_back(std::move(overload));
}

Object_ptr Pocket::get_overload(int overload_id) const
{
    Doctor::get().assert(
        overload_id >= 0 && overload_id < static_cast<int>(overloads.size()),
        WaspStage::VM,
        "Overload index out of bounds!"
    );

    return overloads[overload_id];
}

// -----------------------------------------------------------------------------
// Record Object
// ------------------------------------------------------------------------------

Object_ptr RecordObject::get_field(int member_id) const
{
    Doctor::get().assert(
        member_id >= 0 && member_id < static_cast<int>(fields.size()),
        WaspStage::VM,
        "Record field index out of bounds!"
    );

    return fields[member_id];
}

void RecordObject::set_field(int member_id, Object_ptr value)
{
    Doctor::get().fatal_if_nullptr(value, WaspStage::VM);

    Doctor::get().assert(
        member_id >= 0 && member_id < static_cast<int>(fields.size()),
        WaspStage::VM,
        "Record field index out of bounds!"
    );

    Doctor::get().assert(
        !value->is<std::monostate>(),
        WaspStage::VM,
        "Cannot set a Record field to monostate"
    );

    fields[member_id] = std::move(value);
}

// -----------------------------------------------------------------------------
// Bag Object
// -----------------------------------------------------------------------------

Pocket_ptr BagObject::get_overloads_object(int member_id) const
{
    Doctor::get().assert(
        member_id >= 0 && member_id < static_cast<int>(pocket.size()),
        WaspStage::VM,
        "Bag member index out of bounds!"
    );

    return pocket[member_id];
}

// -----------------------------------------------------------------------------
// Module Object
// -----------------------------------------------------------------------------

Object_ptr ModuleObject::get_member(int member_id) const
{
    Doctor::get().assert(
        member_id >= 0 && member_id < static_cast<int>(members.size()),
        WaspStage::VM,
        "Member index out of bounds!"
    );
    return members[member_id];
}

void ModuleObject::set_member(int member_id, Object_ptr value)
{
    Doctor::get().assert(
        member_id >= 0 && member_id < static_cast<int>(members.size()),
        WaspStage::VM,
        "Member index out of bounds!"
    );
    members[member_id] = std::move(value);
}

} // namespace Wasp
