#include "AST.h"
#include "CFGraph.h"
#include "Compiler.h"
#include "Doctor.h"
#include "Objects.h"
#include "OpCode.h"
#include "Statement.h"
#include "Workspace.h"

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

void Compiler::visit(Return& statement)
{
    if (statement.expression.has_value())
    {
        visit(statement.expression.value());
    }
    else
    {
        emit(OpCode::LOAD_NONE);
    }

    emit(OpCode::RETURN);
}

void Compiler::visit(ClassDefinition& class_definition)
{
    int class_blueprint_physical_index = get_or_add_local_index(class_definition.symbol);

    emit(OpCode::PUSH_EMPTY_CLASS_BLUEPRINT, "push empty class " + class_definition.name);
    emit(OpCode::SET_LOCAL, class_blueprint_physical_index, "init class in slot");

    int unique_method_count = 0;

    std::shared_ptr<ClassType> class_type;
    auto type_obj = class_definition.symbol->get_type();

    if (auto template_type = type_obj->try_as<ClassTemplateType_ptr>())
    {
        class_type = (*template_type)->class_type;
    }
    else
    {
        class_type = type_obj->as<ClassType_ptr>();
    }

    StringVector all_methods = class_type->methods;
    all_methods.insert(all_methods.end(), class_type->pures.begin(), class_type->pures.end());
    all_methods.insert(all_methods.end(), class_type->statics.begin(), class_type->statics.end());

    auto compile_if_match = [&](auto& method, const std::string& target_name, int& count)
    {
        if (method.name == target_name)
        {
            compile_function_closure(method.name, method.parameter_symbols, method.body);
            count++;
        }
    };

    for (const std::string& method_name : all_methods)
    {
        int overload_count = 0;

        for (auto& stmt : class_definition.members)
        {
            std::visit(
                overloaded{
                    [&](MethodDefinition& m)
                    {
                        compile_if_match(m, method_name, overload_count);
                    },
                    [&](PureMethodDefinition& m)
                    {
                        compile_if_match(m, method_name, overload_count);
                    },
                    [&](OurMethodDefinition& m)
                    {
                        compile_if_match(m, method_name, overload_count);
                    },
                    [&](OurPureMethodDefinition& m)
                    {
                        compile_if_match(m, method_name, overload_count);
                    },
                    [&](auto&)
                    {
                    }
                },
                stmt->data
            );
        }

        emit(OpCode::BUILD_OVERLOAD_GROUP, overload_count, "overloads for " + method_name);
        unique_method_count++;
    }

    auto fields_offset = static_cast<int>(class_type->fields.size());

    emit(
        OpCode::GET_LOCAL,
        class_blueprint_physical_index,
        "load pre-allocated class for modification"
    );
    emit(
        OpCode::BUILD_CLASS,
        unique_method_count,
        fields_offset,
        "populate class " + class_definition.name
    );
    emit(OpCode::SET_LOCAL, class_blueprint_physical_index, "update local slot");
}

void Compiler::visit(TraitDefinition& trait_definition)
{
}

void Compiler::visit(FunctionDefinition& function_definition)
{
    compile_function_closure(
        function_definition.name,
        function_definition.parameter_symbols,
        function_definition.body
    );

    int physical_index = get_or_add_local_index(function_definition.group_symbol);
    emit(OpCode::STORE_FUNCTION_OVERLOAD, physical_index, "fun " + function_definition.name);
}

void Compiler::visit(PureFunctionDefinition& function_definition)
{
    compile_function_closure(
        function_definition.name,
        function_definition.parameter_symbols,
        function_definition.body
    );

    int physical_index = get_or_add_local_index(function_definition.group_symbol);
    emit(OpCode::STORE_FUNCTION_OVERLOAD, physical_index, "fun " + function_definition.name);
}

void Compiler::emit_closure_upvalues(const std::vector<Upvalue>& upvalues)
{
    emit(OpCode::MAKE_FUNCTION, static_cast<int>(upvalues.size()));

    for (const auto& uv : upvalues)
    {
        emit_raw_byte(uv.is_local_to_parent ? std::byte{1} : std::byte{0});
        emit_raw_byte(static_cast<std::byte>(uv.index));
    }
}

void Compiler::compile_function_closure(
    const std::string& name,
    const std::vector<Symbol_ptr>& parameters,
    StatementVector body
)
{
    Compiler func_compiler(this);

    func_compiler.enter_scope();

    for (const auto& param_symbol : parameters)
    {
        func_compiler.stack.push_back(param_symbol);
    }

    func_compiler.visit(body);
    func_compiler.leave_scope();

    func_compiler.emit(OpCode::LOAD_NONE);
    func_compiler.emit(OpCode::RETURN);

    CodeObject code = func_compiler.flatten();

    int const_id = workspace->pool->allocate_function_definition(std::move(code), name);
    emit(OpCode::LOAD_CONST, const_id, "fun " + name);

    emit_closure_upvalues(func_compiler.upvalues);
}

// TEMPLATES

void Compiler::visit(TemplateDefinition& statement)
{
    Doctor::get()
        .fatal_if_nullptr(statement.target, WaspStage::Compiler, "Template target cannot be null");

    std::visit(
        overloaded{
            [&](FunctionDefinition& f)
            {
                visit(f);
            },
            [&](PureFunctionDefinition& f)
            {
                visit(f);
            },
            [&](ClassDefinition& c)
            {
                visit(c);
            },
            [&](auto&)
            {
                Doctor::get().fatal(WaspStage::Compiler, "Invalid template target");
            }
        },
        statement.target->data
    );
}

} // namespace Wasp
