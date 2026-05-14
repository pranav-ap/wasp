#include <functional>
#include <memory>
#include <string>

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

void SemanticAnalyzer::visit(EnumDefinition& def)
{
    int global_enum_value = 0;

    std::function<EnumType_ptr(const EnumDefinition&, const std::string&)>
        build_enum = [&](const EnumDefinition& e_def,
                         const std::string& prefix) -> EnumType_ptr
    {
        std::string current_name = prefix.empty() ? e_def.name
                                                  : prefix + "." + e_def.name;
        auto enum_type = std::make_shared<EnumType>(current_name);

        for (const auto& [name, old_val] : e_def.members)
        {
            enum_type->members[current_name + "." + name] = global_enum_value++;
        }

        for (const auto& nested_def : e_def.nested_enums)
        {
            enum_type->nested_enums
                [current_name + "." + nested_def.name] = build_enum(
                nested_def,
                current_name
            );
        }

        return enum_type;
    };

    auto enum_type = build_enum(def, "");
    def.symbol->set_type(make_object(enum_type));
}

} // namespace Wasp
