#include "AST.h"
#include "Doctor.h"
#include "Expression.h"
#include "Objects.h"
#include "SemanticAnalyzer.h"
#include "Workspace.h"

#include <algorithm>
#include <cctype>
#include <ctime>
#include <memory>
#include <optional>
#include <string>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{
namespace
{
StringVector unfurl_member_access(const MemberAccess& expr)
{
    StringVector path = {expr.right->as<Identifier>().name};
    Expression_ptr current = expr.left;

    while (current && current->is<MemberAccess>())
    {
        auto& nested_ma = current->as<MemberAccess>();
        if (!nested_ma.right->is<Identifier>())
        {
            break;
        }

        path.push_back(nested_ma.right->as<Identifier>().name);
        current = nested_ma.left;
    }

    if (current && current->is<Identifier>())
    {
        path.push_back(current->as<Identifier>().name);
    }

    std::reverse(path.begin(), path.end());
    return path;
}
} // namespace

std::optional<Object_ptr> SemanticAnalyzer::try_resolve_as_enum(
    MemberAccess& ma
)
{
    StringVector path = unfurl_member_access(ma);

    if (path.size() < 2)
    {
        return std::nullopt;
    }

    Symbol_ptr base_sym = current_scope->lookup(path.front());
    if (!base_sym || !base_sym->get_type())
    {
        return std::nullopt;
    }

    Object_ptr base_type = base_sym->get_type();
    if (base_type->is<TypeAlias_ptr>())
    {
        base_type = base_type->unwrap_type_alias();
    }

    if (!base_type->is<EnumType_ptr>())
    {
        return std::nullopt;
    }

    auto enum_type = base_type->as<EnumType_ptr>();
    int value = enum_type->get_value(path);

    Doctor::get()
        .assert(value != -1, WaspStage::Semantics, "Enum member not found.");

    ma.is_enum_value = true;
    ma.enum_member_value = value;
    ma.enum_type_id = enum_type->type_id;

    return make_object(std::make_shared<EnumMemberType>(enum_type, value));
}

} // namespace Wasp
