#include "Doctor.h"
#include "Objects.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <string>
#include <utility>

namespace Wasp
{

void OopsType::add_overload(const std::string& member_name, Object_ptr overload)
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

ObjectVector OopsType::get_overloads(const std::string& member_name) const
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

ObjectVector OopsType::get_fields() const
{
    ObjectVector res;
    for (const auto& n : fields)
    {
        res.push_back(member_types.at(n));
    }
    return res;
}

ObjectVector OopsType::get_methods() const
{
    ObjectVector res;
    for (const auto& n : methods)
    {
        res.push_back(member_types.at(n));
    }
    return res;
}

ObjectVector OopsType::get_pures() const
{
    ObjectVector res;
    for (const auto& n : pures)
    {
        res.push_back(member_types.at(n));
    }
    return res;
}

ObjectVector OopsType::get_statics() const
{
    ObjectVector res;
    for (const auto& n : statics)
    {
        res.push_back(member_types.at(n));
    }
    return res;
}

ObjectVector OopsType::get_members() const
{
    ObjectVector res = get_fields();
    ObjectVector m = get_methods();
    res.insert(res.end(), m.begin(), m.end());

    return res;
}

bool OopsType::is_pure(const std::string& member_name) const
{
    Doctor::get().assert(
        contains_member(member_name),
        WaspStage::Semantics,
        "Member not found."
    );
    return std::find(pures.begin(), pures.end(), member_name) != pures.end();
}

bool OopsType::is_static(const std::string& member_name) const
{
    Doctor::get().assert(
        contains_member(member_name),
        WaspStage::Semantics,
        "Member not found."
    );
    return std::find(statics.begin(), statics.end(), member_name) != statics.end();
}

bool OopsType::contains_member(const std::string& member_name) const
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

Object_ptr OopsType::get_member(const std::string& member_name) const
{
    Doctor::get().assert(
        contains_member(member_name),
        WaspStage::Semantics,
        "Type Member '" + member_name + "' not found in OOP Type '" + name + "'!"
    );

    return member_types.at(member_name);
}

void OopsType::set_member(const std::string& member_name, Object_ptr value)
{
    Doctor::get().assert(
        contains_member(member_name),
        WaspStage::Semantics,
        "Cannot set value for unregistered member '" + member_name +
            "' in OOP Type '" + name + "'!"
    );

    member_types[member_name] = std::move(value);
}

int OopsType::get_member_index(const std::string& member_name) const
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

Object_ptr OopsType::get_member(int member_id) const
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

ObjectVector OopsType::get_traits() const
{
    return traits;
}

bool OopsType::implements_trait(const std::string& trait_name) const
{
    for (const auto& trait_obj : traits)
    {
        auto trait_type = trait_obj->as<TraitType_ptr>();
        if (trait_type->name == trait_name)
        {
            return true;
        }
    }

    return false;
}

} // namespace Wasp
