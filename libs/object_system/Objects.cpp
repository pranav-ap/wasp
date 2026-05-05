#include "Objects.h"
#include "Doctor.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>

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

bool BaseMemberedType::contains_member(const std::string& member_name) const
{
    return member_types.contains(member_name);
}

Object_ptr BaseMemberedType::get_member(const std::string& member_name) const
{
    Doctor::get().assert(
        contains_member(member_name),
        WaspStage::Semantics,
        "Type Member '" + member_name + "' not found!"
    );
    return member_types.at(member_name);
}

void BaseMemberedType::set_member(const std::string& member_name, Object_ptr value)
{
    member_types[member_name] = std::move(value);
}

Object_ptr ModuleType::get_member(int member_id) const
{
    Doctor::get().assert(
        member_id >= 0 && member_id < static_cast<int>(ordered_keys.size()),
        WaspStage::Semantics,
        "Type Member index out of bounds!"
    );
    return member_types.at(ordered_keys[member_id]);
}

void ModuleType::set_member(int member_id, Object_ptr value)
{
    Doctor::get().assert(
        member_id >= 0 && member_id < static_cast<int>(ordered_keys.size()),
        WaspStage::Semantics,
        "Type Member index out of bounds!"
    );
    member_types[ordered_keys[member_id]] = std::move(value);
}

int ModuleType::get_member_index(const std::string& member_name) const
{
    Doctor::get().assert(
        contains_member(member_name),
        WaspStage::Semantics,
        "Member '" + member_name + "' not found!"
    );

    for (size_t i = 0; i < ordered_keys.size(); ++i)
    {
        if (ordered_keys[i] == member_name)
            return static_cast<int>(i);
    }
    return -1;
}

void BaseOOPType::add_overload(const std::string& member_name, Object_ptr overload)
{
    if (!member_types.contains(member_name))
    {
        member_types[member_name] = make_object(
            std::make_shared<ObjectOverloadList>()
        );
    }

    auto overload_list = member_types.at(member_name)->as<ObjectOverloadList_ptr>();
    overload_list->add_overload(std::move(overload));
}

ObjectVector BaseOOPType::get_overloads(const std::string& member_name) const
{
    Doctor::get().assert(
        contains_member(member_name) &&
            member_types.at(member_name)->is<ObjectOverloadList_ptr>(),
        WaspStage::Semantics,
        "Expected an ObjectOverloadList for member '" + member_name + "' in " + name
    );

    auto overload_list = member_types.at(member_name)->as<ObjectOverloadList_ptr>();
    return overload_list->overloads;
}

ObjectVector BaseOOPType::get_methods() const
{
    ObjectVector method_types;
    for (const auto& method_name : methods)
        method_types.push_back(member_types.at(method_name));
    return method_types;
}

ObjectVector BaseOOPType::get_pures() const
{
    ObjectVector pure_types;
    for (const auto& pure_name : pures)
        pure_types.push_back(member_types.at(pure_name));
    return pure_types;
}

ObjectVector BaseOOPType::get_statics() const
{
    ObjectVector static_types;
    for (const auto& static_name : statics)
        static_types.push_back(member_types.at(static_name));
    return static_types;
}

bool BaseOOPType::is_pure(std::string member_name) const
{
    Doctor::get().assert(contains_member(member_name), WaspStage::Semantics, "Member not found.");
    return std::find(pures.begin(), pures.end(), member_name) != pures.end();
}

bool BaseOOPType::is_static(std::string member_name) const
{
    Doctor::get().assert(contains_member(member_name), WaspStage::Semantics, "Member not found.");
    return std::find(statics.begin(), statics.end(), member_name) != statics.end();
}

int ClassType::get_member_index(const std::string& member_name) const
{
    Doctor::get().assert(contains_member(member_name), WaspStage::Semantics, "Member not found.");

    for (size_t i = 0; i < fields.size(); ++i)
        if (fields[i] == member_name)
            return static_cast<int>(i);

    for (size_t i = 0; i < methods.size(); ++i)
        if (methods[i] == member_name)
            return static_cast<int>(fields.size() + i);

    for (size_t i = 0; i < pures.size(); ++i)
        if (pures[i] == member_name)
            return static_cast<int>(fields.size() + methods.size() + i);

    return -1;
}

Object_ptr ClassType::get_member(int member_id) const
{
    int field_count = static_cast<int>(fields.size());
    if (member_id < field_count)
        return member_types.at(fields[member_id]);

    int method_index = member_id - field_count;
    if (method_index < static_cast<int>(methods.size()))
        return member_types.at(methods[method_index]);

    int pure_index = method_index - static_cast<int>(methods.size());
    if (pure_index < static_cast<int>(pures.size()))
        return member_types.at(pures[pure_index]);

    Doctor::get().fatal(WaspStage::Semantics, "ClassType member index out of bounds!");
}

ObjectVector ClassType::get_fields() const
{
    ObjectVector field_types;
    for (const auto& field_name : fields)
        field_types.push_back(member_types.at(field_name));
    return field_types;
}

ObjectVector ClassType::get_members() const
{
    ObjectVector all_members = get_fields();
    ObjectVector methods_vec = get_methods();
    ObjectVector pures_vec = get_pures();

    all_members.insert(all_members.end(), methods_vec.begin(), methods_vec.end());
    all_members.insert(all_members.end(), pures_vec.begin(), pures_vec.end());

    return all_members;
}

int TraitType::get_member_index(const std::string& member_name) const
{
    Doctor::get().assert(contains_member(member_name), WaspStage::Semantics, "Member not found.");

    for (size_t i = 0; i < methods.size(); ++i)
        if (methods[i] == member_name)
            return static_cast<int>(i);

    for (size_t i = 0; i < pures.size(); ++i)
        if (pures[i] == member_name)
            return static_cast<int>(methods.size() + i);

    return -1;
}

Object_ptr TraitType::get_member(int member_id) const
{
    if (member_id < static_cast<int>(methods.size()))
        return member_types.at(methods[member_id]);

    int pure_index = member_id - static_cast<int>(methods.size());
    if (pure_index < static_cast<int>(pures.size()))
        return member_types.at(pures[pure_index]);

    Doctor::get().fatal(WaspStage::Semantics, "TraitType member index out of bounds!");
    return nullptr;
}

ObjectVector TraitType::get_members() const
{
    ObjectVector all_members;
    ObjectVector methods_vec = get_methods();
    ObjectVector pures_vec = get_pures();

    all_members.insert(all_members.end(), methods_vec.begin(), methods_vec.end());
    all_members.insert(all_members.end(), pures_vec.begin(), pures_vec.end());

    return all_members;
}

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

} // namespace Wasp
