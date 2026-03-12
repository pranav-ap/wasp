#include "SymbolHoister.h"
#include "Doctor.h"
#include "Expression.h"
#include "Statement.h"
#include "Workspace.h"

#include <memory>
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
                    Doctor::get().assert_true(
                        !module->exports.contains(func_def.name),
                        WaspStage::Captain,
                        "Duplicate export: '" + func_def.name + "' is already defined."
                    );

                    auto symbol = std::make_shared<Symbol>(
                        func_def.name,
                        nullptr, // Phase 3 will resolve the FunctionType
                        false,   // is_mutable
                        false,   // is_native
                        false,   // is_captured
                        0,       // closure_depth (Globals are always 0)
                        0        // lexical_depth (Globals are always 0)
                    );

                    module->exports[func_def.name] = symbol;
                },

                [&](const ClassDefinition& class_def) {
                    Doctor::get().assert_true(
                        !module->exports.contains(class_def.name),
                        WaspStage::Captain,
                        "Duplicate export: '" + class_def.name + "' is already defined."
                    );

                    auto symbol = std::make_shared<Symbol>(
                        class_def.name, nullptr, false, false, false, 0, 0
                    );

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

                    Doctor::get().assert_true(
                        var_name != "" && !module->exports.contains(var_name),
                        WaspStage::Captain,
                        "Duplicate export: Global variable '" + var_name + "' is already defined."
                    );

                    auto symbol = std::make_shared<Symbol>(
                        var_name,
                        nullptr, // Phase 3 will resolve the RHS Object_ptr type
                        var_def.is_mutable,
                        false,
                        false,
                        0,
                        0
                    );

                    module->exports[var_name] = symbol;
                },

                [&](const EnumDefinition& enum_def) {
                    Doctor::get().assert_true(
                        !module->exports.contains(enum_def.name),
                        WaspStage::Captain,
                        "Duplicate export: '" + enum_def.name + "' is already defined."
                    );

                    auto symbol =
                        std::make_shared<Symbol>(enum_def.name, nullptr, false, false, false, 0, 0);

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