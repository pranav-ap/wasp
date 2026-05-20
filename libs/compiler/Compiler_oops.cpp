#include "Compiler.h"
#include "AST.h"
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
    int unique_method_count = 0;

    int class_type_pool_id = workspace->pool->allocate_type(class_type_obj);

    for (const std::string& method_name : class_type->methods)
    {
        int overload_count = 0;

        for (auto& stmt : def.members)
        {
            auto* func = stmt->try_as<MethodDefinition>();

            if (!func || func->name != method_name)
            {
                continue;
            }

            if (func->symbol->is_native())
            {
                std::string mangled = mangle_name(func->name, def.name, func->symbol->module_path);

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
        unique_method_count++;
    }

    emit(OpCode::LOAD_CONST, class_type_pool_id, "load class type from pool");

    emit(OpCode::GET_LOCAL, slot, "load blueprint for population");
    emit(
        OpCode::BUILD_CLASS,
        unique_method_count,
        static_cast<int>(class_type->fields.size()),
        "populate class " + def.name
    );
    emit(OpCode::SET_LOCAL, slot, "update local slot");
}

} // namespace Wasp
