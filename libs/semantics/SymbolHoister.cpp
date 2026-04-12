#include "SymbolHoister.h"
#include "Doctor.h"
#include "Expression.h"
#include "Statement.h"
#include "Workspace.h"

#include <memory>
#include <string>
#include <utility>
#include <variant>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

void SymbolHoister::hoist(Module_ptr mod)
{
    for (const auto& stmt_ptr : mod->stmts)
    {
        std::visit(
            overloaded{
                [&](LocalFunctionDefinition& func_def)
                {
                    auto symbol = SymbolFactory::create_local_function(
                        func_def.name,
                        nullptr,
                        false
                    );
                    current_scope->define(symbol);
                    func_def.symbol = symbol;
                },

                [&](ClassDefinition& class_def)
                {
                    auto symbol = SymbolFactory::create_class(class_def.name, nullptr);
                    current_scope->define(symbol);
                    class_def.symbol = symbol;
                },

                [&](VariableDefinition& var_def)
                {
                    std::string var_name = "";

                    if (var_def.expression->is<UntypedAssignment>())
                    {
                        var_name = var_def.expression->as<UntypedAssignment>()
                                       .lhs_expression->as<Identifier>()
                                       .name;
                    }
                    else if (var_def.expression->is<TypedAssignment>())
                    {
                        var_name = var_def.expression->as<TypedAssignment>()
                                       .lhs_expression->as<Identifier>()
                                       .name;
                    }

                    Doctor::get().assert(
                        !var_name.empty(),
                        WaspStage::Semantics,
                        "Expected variable definition to have an identifier on the LHS");

                    auto symbol = SymbolFactory::create_variable(
                        var_name,
                        nullptr,
                        var_def.is_mutable);

                    current_scope->define(symbol);
                    var_def.symbol = symbol;
                },

                [&](EnumDefinition& enum_def)
                {
                    auto symbol = SymbolFactory::create_enum(enum_def.name, nullptr);
                    current_scope->define(symbol);
                    enum_def.symbol = symbol;
                },

                // Ignore other statements
                [](auto&)
                {
                }
            },
            stmt_ptr->data
        );
    }
}

void SymbolHoister::run(Module_ptr mod)
{
    hoist(mod);

    auto hoisted_symbols = current_scope->get_all_symbols();
    mod->exports = std::move(hoisted_symbols);
}
} // namespace Wasp
