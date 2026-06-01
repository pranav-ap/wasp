#include "AST.h"
#include "Doctor.h"
#include "Objects.h"
#include "SemanticAnalyzer.h"
#include "Workspace.h"

#include <algorithm>
#include <ctime>
#include <memory>
#include <variant>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

StringVector SemanticAnalyzer::setup_ordered_export_names(Module_ptr mod)
{
    StringVector ordered_export_names;

    for (auto& stmt_ptr : mod->stmts)
    {
        std::visit(
            overloaded{[&](auto& def)
                       {
                           if constexpr (requires { def.name; })
                           {
                               if (std::find(
                                       ordered_export_names.begin(),
                                       ordered_export_names.end(),
                                       def.name
                                   ) == ordered_export_names.end())
                               {
                                   ordered_export_names.push_back(def.name);
                               }
                           }
                       }},
            stmt_ptr->data
        );
    }

    return ordered_export_names;
}

void SemanticAnalyzer::setup_exports(
    Module_ptr mod,
    StringVector ordered_export_names
)
{
    for (const auto& name : ordered_export_names)
    {
        auto symbol = current_scope->lookup(name);

        if (symbol && symbol->is_exportable())
        {
            Doctor::get().fatal_if_nullptr(
                symbol->get_type(),
                WaspStage::Semantics,
                "Symbol '" + name + "' has no type information"
            );

            mod->exports.push_back(symbol);
        }
    }
}

void SemanticAnalyzer::extract_module_type(Module_ptr mod)
{
    ObjectStringMap member_types;
    StringVector ordered_keys;

    for (const auto& symbol : mod->exports)
    {
        Doctor::get().fatal_if_nullptr(
            symbol->get_type(),
            WaspStage::Semantics,
            "Symbol '" + symbol->name + "' has no type information"
        );

        member_types[symbol->name] = symbol->get_type();
        ordered_keys.push_back(symbol->name);
    }

    mod->type = std::make_shared<Object>(std::make_shared<ModuleType>(
        mod->get_name(),
        member_types,
        ordered_keys
    ));
}

} // namespace Wasp
