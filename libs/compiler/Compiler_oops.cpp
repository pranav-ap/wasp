#include "AST.h"
#include "Compiler.h"
#include "Objects.h"
#include "OpCode.h"
#include "Statement.h"
#include "Workspace.h"

#include <memory>
#include <string>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

void Compiler::visit(ClassDefinition& def)
{
    int slot = get_or_add_local_index(def.symbol);

    emit(OpCode::PUSH_EMPTY_CLASS_BLUEPRINT, "class " + def.name);
    emit(OpCode::SET_LOCAL, slot, "init class in slot");

    auto class_type_obj = def.symbol->get_type();
    auto class_type = class_type_obj->as<ClassType_ptr>();

    const auto& method_names = class_type->bag_type->ordered_keys;
    int unique_method_count = static_cast<int>(method_names.size());

    // Allocate class type in constant pool
    int class_type_pool_id = workspace->pool->allocate_type(class_type_obj);

    // Compile each method
    for (const std::string& method_name : method_names)
    {
        int overload_count = 0;

        // Find all methods with this name in the class members
        for (auto& stmt : def.members)
        {
            auto* func = stmt->try_as<FunctionDefinition>();

            if (!func || func->name != method_name)
            {
                continue;
            }

            if (func->symbol->is_native())
            {
                std::string mangled = mangle_name(
                    func->name,
                    def.name,
                    func->symbol->module_path
                );

                int registry_id = workspace->native_registry->get_native_index(
                    mangled
                );

                emit(OpCode::GET_NATIVE, registry_id, "load native " + mangled);
            }
            else
            {
                compile_function_closure(
                    func->name,
                    func->parameter_symbols,
                    func->body,
                    func->context_symbol
                );
            }

            overload_count++;
        }

        emit(
            OpCode::BUILD_OVERLOAD_GROUP,
            overload_count,
            "overloads for " + method_name
        );
    }

    // Build the class at runtime

    emit(
        OpCode::LOAD_CONSTANT,
        class_type_pool_id,
        "load class type from pool"
    );

    emit(OpCode::GET_LOCAL, slot, "load class blueprint");

    emit(
        OpCode::BUILD_CLASS,
        unique_method_count,
        "populate class " + def.name
    );

    emit(OpCode::SET_LOCAL, slot, "update local slot");
}

void Compiler::visit(TraitDefinition& statement)
{
    int physical_index = get_or_add_local_index(statement.symbol);

    if (physical_index != -1)
    {
        // Load none as placeholder for compile-time only value
        emit(OpCode::LOAD_NONE);
        emit(
            OpCode::SET_LOCAL,
            physical_index,
            "compile-time trait: " + statement.name
        );
    }
}

} // namespace Wasp
