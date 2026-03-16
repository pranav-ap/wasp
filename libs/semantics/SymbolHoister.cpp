#include "SymbolHoister.h"
#include "Doctor.h"
#include "Expression.h"
#include "Statement.h"
#include "Symbol.h"
#include "Workspace.h"

#include <string>
#include <variant>
#include <vector>

template <class... Ts> struct overloaded : Ts... {
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp {

void SymbolHoister::run(const std::vector<Module_ptr>& build_order) {
    for (const auto& module : build_order) {
        run(module);
    }
}

void SymbolHoister::run(Module_ptr module) {
    for (const auto& stmt_ptr : module->block) {
        std::visit(
            overloaded{
                [&](const FunctionDefinition& func_def) {
                    Doctor::get().assert(
                        !module->exports.contains(func_def.name),
                        WaspStage::Captain,
                        "Duplicate export: '" + func_def.name + "' is already defined."
                    );

                    auto symbol = SymbolFactory::create_function(
                        func_def.name,
                        nullptr, // Phase 3 will resolve the FunctionType
                        false    // is_native
                    );

                    module->exports[func_def.name] = symbol;
                },

                [&](const ClassDefinition& class_def) {
                    Doctor::get().assert(
                        !module->exports.contains(class_def.name),
                        WaspStage::Captain,
                        "Duplicate export: '" + class_def.name + "' is already defined."
                    );

                    auto symbol = SymbolFactory::create_class(class_def.name, nullptr);

                    module->exports[class_def.name] = symbol;
                },

                [&](const VariableDefinition& var_def) {
                    std::string var_name = "";

                    if (var_def.expression->is<UntypedAssignment>()) {
                        var_name = var_def.expression->as<UntypedAssignment>()
                                       .lhs_expression->as<Identifier>()
                                       .name;
                    } else if (var_def.expression->is<TypedAssignment>()) {
                        var_name = var_def.expression->as<TypedAssignment>()
                                       .lhs_expression->as<Identifier>()
                                       .name;
                    }

                    Doctor::get().assert(
                        var_name != "" && !module->exports.contains(var_name),
                        WaspStage::Captain,
                        "Duplicate export: Global variable '" + var_name + "' is already defined."
                    );

                    auto symbol = SymbolFactory::create_variable(
                        var_name,
                        nullptr, // Phase 3 will resolve the RHS Object_ptr type
                        var_def.is_mutable
                    );

                    module->exports[var_name] = symbol;
                },

                [&](const EnumDefinition& enum_def) {
                    Doctor::get().assert(
                        !module->exports.contains(enum_def.name),
                        WaspStage::Captain,
                        "Duplicate export: '" + enum_def.name + "' is already defined."
                    );

                    auto symbol = SymbolFactory::create_enum(enum_def.name, nullptr);

                    module->exports[enum_def.name] = symbol;
                },

                // Ignore other statements
                [](const auto&) {}
            },
            stmt_ptr->data
        );
    }
}

} // namespace Wasp