#include "Doctor.h"
#include "Objects.h"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

namespace Wasp
{

bool OverloadCoordinate::operator<(const OverloadCoordinate& other) const
{
    if (member_index != other.member_index)
    {
        return member_index < other.member_index;
    }

    return overload_index < other.overload_index;
}

bool OverloadCoordinate::operator==(const OverloadCoordinate& other) const
{
    return member_index == other.member_index &&
           overload_index == other.overload_index;
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
