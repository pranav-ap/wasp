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

    Doctor::get().assert(
        type_alias_obj->is<TypeAlias_ptr>(),
        WaspStage::Semantics,
        "Expected TypeAlias_ptr for type alias definition"
    );

    auto type_alias_type = type_alias_obj->as<TypeAlias_ptr>();

    Object_ptr aliased_type = visit(def.ref_type);
    aliased_type = aliased_type->unwrap_type_alias();
    type_alias_type->underlying_type = aliased_type;
}

StringVector collect_enum_names(
    const EnumDefinition& def,
    const std::string& prefix
)
{
    std::string current_prefix = prefix.empty() ? def.name
                                                : prefix + "." + def.name;

    StringVector out_list;

    // Add current members
    for (const auto& member : def.members)
    {
        out_list.push_back(current_prefix + "." + member);
    }

    // Recurse into nested enums
    for (const auto& nested : def.nested_enums)
    {
        auto nested_names = collect_enum_names(nested, current_prefix);
        out_list
            .insert(out_list.end(), nested_names.begin(), nested_names.end());
    }

    return out_list;
}

void SemanticAnalyzer::visit(EnumDefinition& def)
{
    StringVector full_names = collect_enum_names(def, "");

    auto enum_type_obj = def.symbol->get_type();
    Doctor::get().fatal_if_nullptr(enum_type_obj, WaspStage::Semantics);

    Doctor::get().assert(
        enum_type_obj->is<EnumType_ptr>(),
        WaspStage::Semantics,
        "Expected EnumType_ptr for enum definition"
    );

    auto enum_type = enum_type_obj->as<EnumType_ptr>();
    enum_type->members = std::move(full_names);
}

} // namespace Wasp
