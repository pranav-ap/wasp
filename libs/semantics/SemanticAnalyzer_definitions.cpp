#include <memory>
#include <string>
#include <utility>

#include "AST.h"
#include "Doctor.h"
#include "Objects.h"
#include "SemanticAnalyzer.h"
#include "Statement.h"

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

void SemanticAnalyzer::visit(TypeAliasDefinition& def)
{
    auto type_alias_obj = def.symbol->get_type();
    Doctor::get().fatal_if_nullptr(
        type_alias_obj,
        WaspStage::Semantics,
        "Type alias symbol has no type"
    );

    auto type_alias_type = type_alias_obj->as<TypeAlias_ptr>();

    Object_ptr aliased_type = visit(def.ref_type);
    type_alias_type->underlying_type = aliased_type;
}

void collect_enum_names(
    const EnumDefinition& def,
    const std::string& prefix,
    StringVector& out_list
)
{
    std::string current_prefix = prefix.empty() ? def.name : prefix + "." + def.name;

    // Add current members
    for (const auto& member : def.members)
    {
        out_list.push_back(current_prefix + "." + member);
    }

    // Recurse into nested enums
    for (const auto& nested : def.nested_enums)
    {
        collect_enum_names(nested, current_prefix, out_list);
    }
}

void SemanticAnalyzer::visit(EnumDefinition& def)
{
    StringVector full_names;
    collect_enum_names(def, "", full_names);

    auto enum_type_obj = def.symbol->get_type();
    Doctor::get().fatal_if_nullptr(enum_type_obj, WaspStage::Semantics);

    auto enum_type = enum_type_obj->as<EnumType_ptr>();
    Doctor::get().fatal_if_nullptr(enum_type, WaspStage::Semantics);

    enum_type->members = std::move(full_names);
}

} // namespace Wasp
