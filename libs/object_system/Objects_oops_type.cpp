#include "Doctor.h"
#include "Objects.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <string>
#include <utility>

namespace Wasp
{

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

int ModuleType::get_member_index(const std::string& member_name) const
{
    Doctor::get().assert(
        contains_member(member_name),
        WaspStage::Semantics,
        "Member '" + member_name + "' not found!"
    );

    auto it = std::find(ordered_keys.begin(), ordered_keys.end(), member_name);
    return it != ordered_keys.end() ? std::distance(ordered_keys.begin(), it) : -1;
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

ObjectVector BaseOOPType::get_fields() const
{
    ObjectVector res;
    for (const auto& n : fields)
    {
        res.push_back(member_types.at(n));
    }
    return res;
}

ObjectVector BaseOOPType::get_methods() const
{
    ObjectVector res;
    for (const auto& n : methods)
    {
        res.push_back(member_types.at(n));
    }
    return res;
}

ObjectVector BaseOOPType::get_pures() const
{
    ObjectVector res;
    for (const auto& n : pures)
    {
        res.push_back(member_types.at(n));
    }
    return res;
}

ObjectVector BaseOOPType::get_statics() const
{
    ObjectVector res;
    for (const auto& n : statics)
    {
        res.push_back(member_types.at(n));
    }
    return res;
}

ObjectVector BaseOOPType::get_members() const
{
    ObjectVector res = get_fields();
    ObjectVector m = get_methods();
    res.insert(res.end(), m.begin(), m.end());

    return res;
}

bool BaseOOPType::is_pure(const std::string& member_name) const
{
    Doctor::get().assert(
        contains_member(member_name),
        WaspStage::Semantics,
        "Member not found."
    );
    return std::find(pures.begin(), pures.end(), member_name) != pures.end();
}

bool BaseOOPType::is_static(const std::string& member_name) const
{
    Doctor::get().assert(
        contains_member(member_name),
        WaspStage::Semantics,
        "Member not found."
    );
    return std::find(statics.begin(), statics.end(), member_name) != statics.end();
}

bool BaseOOPType::contains_member(const std::string& member_name) const
{
    bool in_fields = std::find(fields.begin(), fields.end(), member_name) !=
                     fields.end();
    bool in_methods = std::find(methods.begin(), methods.end(), member_name) !=
                      methods.end();
    bool in_pures = std::find(pures.begin(), pures.end(), member_name) !=
                    pures.end();
    bool in_statics = std::find(statics.begin(), statics.end(), member_name) !=
                      statics.end();

    return in_fields || in_methods || in_pures || in_statics;
}

Object_ptr BaseOOPType::get_member(const std::string& member_name) const
{
    Doctor::get().assert(
        contains_member(member_name),
        WaspStage::Semantics,
        "Type Member '" + member_name + "' not found in OOP Type '" + name + "'!"
    );

    return member_types.at(member_name);
}

void BaseOOPType::set_member(const std::string& member_name, Object_ptr value)
{
    Doctor::get().assert(
        contains_member(member_name),
        WaspStage::Semantics,
        "Cannot set value for unregistered member '" + member_name +
            "' in OOP Type '" + name + "'!"
    );

    member_types[member_name] = std::move(value);
}

int BaseOOPType::get_member_index(const std::string& member_name) const
{
    Doctor::get().assert(
        contains_member(member_name),
        WaspStage::Semantics,
        "Member not found."
    );

    int offset = 0;
    auto check_section = [&](const StringVector& vec) -> int
    {
        auto it = std::find(vec.begin(), vec.end(), member_name);
        if (it != vec.end())
        {
            return offset + std::distance(vec.begin(), it);
        }
        offset += vec.size();
        return -1;
    };

    int res = check_section(fields);
    if (res != -1)
    {
        return res;
    }
    res = check_section(methods);
    if (res != -1)
    {
        return res;
    }
    res = check_section(pures);
    if (res != -1)
    {
        return res;
    }
    res = check_section(statics);
    return res;
}

Object_ptr BaseOOPType::get_member(int member_id) const
{
    auto check_section = [&](const StringVector& vec, int& id) -> Object_ptr
    {
        if (id < static_cast<int>(vec.size()))
        {
            return member_types.at(vec[id]);
        }
        id -= vec.size();
        return nullptr;
    };

    int id = member_id;
    if (auto obj = check_section(fields, id))
    {
        return obj;
    }
    if (auto obj = check_section(methods, id))
    {
        return obj;
    }
    if (auto obj = check_section(pures, id))
    {
        return obj;
    }
    if (auto obj = check_section(statics, id))
    {
        return obj;
    }

    Doctor::get().fatal(
        WaspStage::Semantics,
        "BaseOOPType member index out of bounds!"
    );
}

} // namespace Wasp
