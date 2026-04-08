#include "Workspace.h"
#include "AST.h"
#include "ConstantPool.h"
#include "Doctor.h"
#include "NativeRegistry.h"
#include "Objects.h"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <iterator>
#include <map>
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

// ---------------------------------------------------------------------
// Overload Group Data
// ---------------------------------------------------------------------

SymbolVector OverloadGroupData::get_all_overloads() const
{
    SymbolVector all_overloads = siblings;

    for (const auto& parent : parents)
    {
        all_overloads.push_back(parent);
    }

    return all_overloads;
}

bool OverloadGroupData::is_native() const
{
    for (const auto& sibling : siblings)
    {
        if (sibling->payload_is<FunctionData>() &&
            sibling->get_payload_as<FunctionData>().is_native)
        {
            return true;
        }
    }

    for (const auto& parent : parents)
    {
        if (parent->payload_is<FunctionData>() && parent->get_payload_as<FunctionData>().is_native)
        {
            return true;
        }
    }

    return false;
}

// --------------------------------------------------------------------
// Symbol
// --------------------------------------------------------------------

Symbol::Symbol(
    int id,
    std::string name,
    int closure_depth,
    int lexical_depth,
    SymbolPayload payload
)
    : id(id), name(std::move(name)), declaration_depth(closure_depth),
      lexical_depth(lexical_depth), payload(std::move(payload))
{
}

bool Symbol::is_global() const
{
    return lexical_depth == 0;
}

bool Symbol::is_exported() const
{
    return is_global() && (!payload_is<ModuleData>() && !payload_is<AliasData>());
}

bool Symbol::should_be_captured(int usage_depth) const
{
    return declaration_depth < usage_depth;
}

Object_ptr Symbol::get_type()
{
    return std::visit(
        ::overloaded{
            [](const VariableData& d)
            {
                return d.type;
            },
            [](const FunctionData& d)
            {
                return d.type;
            },
            [](const ClassData& d)
            {
                return d.type;
            },
            [](const EnumData& d)
            {
                return d.type;
            },

            [](const ModuleData& d) -> Object_ptr
            {
                return d.mod->type;
            },

            [](const AliasData& d)
            {
                return d.target->get_type();
            },

            [](OverloadGroupData& d)
            {
                if (d.type)
                {
                    return d.type;
                }

                ObjectVector overload_types;

                for (const auto& overload : d.get_all_overloads())
                {
                    overload_types.push_back(overload->get_type());
                }

                d.type = make_object(
                    std::make_shared<OverloadedTypesSet>(
                        std::move(overload_types)
                    )
                );

                return d.type;
            },
            [](auto) -> Object_ptr
            {
                Doctor::get().fatal(
                    WaspStage::Semantics,
                    "This symbol does not have type information");
                return nullptr;
            }
        },
        payload
    );
}

void Symbol::set_type(Object_ptr new_type)
{
    std::visit(
        ::overloaded{
            [&](VariableData& d)
            {
                d.type = new_type;
            },
            [&](FunctionData& d)
            {
                d.type = new_type;
            },
            [&](ClassData& d)
            {
                d.type = new_type;
            },
            [&](EnumData& d)
            {
                d.type = new_type;
            },
            [&](AliasData& d)
            {
                d.target->set_type(new_type);
            },
            [](auto) -> void
            {
                Doctor::get().fatal(
                    WaspStage::Semantics,
                    "You are not allowed to set type for this object");
            }
        },
        payload
    );
}

Symbol_ptr Symbol::resolve()
{
    if (payload_is<AliasData>())
    {
        // Recursively unwrap aliases until we hit the real symbol
        return get_payload_as<AliasData>().target->resolve();
    }

    return shared_from_this();
}

// --------------------------------------------------------------------
// Payloads
// --------------------------------------------------------------------

Object_ptr FunctionData::get_return_type() const
{
    Doctor::get().assert(
        type->is<FunctionType>(),
        WaspStage::Semantics,
        "FunctionData's type payload is not a FunctionType");

    const auto& signature = type->as<FunctionType>();

    if (signature.return_type.has_value())
    {
        return signature.return_type.value();
    }

    return make_object(NoneType());
}

int OverloadGroupData::get_overload_index(const Symbol_ptr& target) const
{
    SymbolVector all_overloads = get_all_overloads();
    auto it = std::find(all_overloads.begin(), all_overloads.end(), target);

    Doctor::get().assert(
        it != all_overloads.end(),
        WaspStage::Semantics,
        "Target symbol not found in overload group"
    );

    return static_cast<int>(std::distance(all_overloads.begin(), it));
}

// --------------------------------------------------------------------
// SymbolFactory
// --------------------------------------------------------------------

Symbol_ptr SymbolFactory::create_variable(
    std::string name,
    Object_ptr type,
    bool is_mutable,
    int closure_depth,
    int lexical_depth)
{
    return std::make_shared<Symbol>(
        symbol_id_counter++,
        std::move(name),
        closure_depth,
        lexical_depth,
        VariableData{std::move(type), is_mutable});
}

Symbol_ptr SymbolFactory::create_function(
    std::string name,
    Object_ptr type,
    bool is_native,
    Object_ptr bound_instance_type,
    int closure_depth,
    int lexical_depth
)
{
    auto symbol = std::make_shared<Symbol>(
        symbol_id_counter++,
        std::move(name),
        closure_depth,
        lexical_depth,
        FunctionData{{std::move(type)}, is_native, bound_instance_type}
    );

    return symbol;
}

Symbol_ptr SymbolFactory::create_class(
    std::string name,
    Object_ptr type,
    int closure_depth,
    int lexical_depth)
{
    return std::make_shared<Symbol>(
        symbol_id_counter++,
        std::move(name),
        closure_depth,
        lexical_depth,
        ClassData{{std::move(type)}});
}

Symbol_ptr SymbolFactory::create_enum(
    std::string name,
    Object_ptr type,
    int closure_depth,
    int lexical_depth)
{
    return std::make_shared<Symbol>(
        symbol_id_counter++,
        std::move(name),
        closure_depth,
        lexical_depth,
        EnumData{{std::move(type)}});
}

Symbol_ptr SymbolFactory::create_module(std::string name, Module_ptr mod)
{
    return std::make_shared<Symbol>(
        symbol_id_counter++,
        std::move(name),
        0,
        0,
        ModuleData{std::move(mod)});
}

Symbol_ptr SymbolFactory::create_alias(std::string name, Symbol_ptr target)
{
    return std::make_shared<Symbol>(
        symbol_id_counter++,
        std::move(name),
        0,
        0,
        AliasData{std::move(target)});
}

Symbol_ptr SymbolFactory::create_overload_set(
    std::string name,
    int closure_depth,
    int lexical_depth)
{
    auto symbol = std::make_shared<Symbol>(
        symbol_id_counter++,
        std::move(name),
        closure_depth,
        lexical_depth,
        OverloadGroupData{});

    return symbol;
}

// --------------------------------------------------------------------
// Module
// --------------------------------------------------------------------

Module::Module(std::filesystem::path file_path, StatementVector stmts)
    : absolute_filepath(std::move(file_path)), stmts(std::move(stmts))
{
}

std::string Module::get_name() const
{
    return absolute_filepath.stem().string();
}

int Module::get_member_index(const std::string& member_name) const
{
    for (size_t i = 0; i < exports.size(); i++)
    {
        if (exports[i]->name == member_name)
        {
            return static_cast<int>(i);
        }
    }

    Doctor::get().fatal(
        WaspStage::Semantics,
        "Member '" + member_name + "' not found in module '" + get_name() + "'"
    );

    return -1;
}

Symbol_ptr Module::get_member(const std::string& member_name) const
{
    for (const auto& sym : exports)
    {
        if (sym->name == member_name)
        {
            return sym;
        }
    }

    Doctor::get().fatal(
        WaspStage::Semantics,
        "Member '" + member_name + "' not found in module '" + get_name() + "'"
    );

    return nullptr;
}

// --------------------------------------------------------------------
// Workspace
// --------------------------------------------------------------------

Workspace::Workspace(std::filesystem::path root)
    : root_path(std::filesystem::absolute(root)),
      build_path(root_path / "build"), libs_path(root_path / "libs"),
      pool(std::make_shared<ConstantPool>()),
      native_registry(std::make_shared<NativeRegistry>(pool))
{
    std::filesystem::create_directories(build_path);
    std::filesystem::create_directories(libs_path);
}

Module_ptr Workspace::get_module(const std::filesystem::path& path)
{
    auto abs_path = std::filesystem::absolute(path);

    if (module_registry.contains(abs_path))
    {
        return module_registry.at(abs_path);
    }

    return nullptr;
}

Module_ptr Workspace::get_module(int module_index)
{
    auto it = module_registry.begin();
    std::advance(it, module_index);
    return it->second;
}

const std::map<std::filesystem::path, Module_ptr>& Workspace::
    get_all_modules() const
{
    return module_registry;
}

void Workspace::add_module(const std::filesystem::path& path, Module_ptr module)
{
    auto abs_path = std::filesystem::absolute(path);

    Doctor::get().assert(
        !module_registry.contains(abs_path),
        WaspStage::Compiler,
        "Module already exists in workspace: " + abs_path.string()
    );

    module_registry[abs_path] = std::move(module);
}

int Workspace::get_module_index(const std::filesystem::path& path) const
{
    auto abs_path = std::filesystem::absolute(path);

    auto it = module_registry.find(abs_path);
    Doctor::get().assert(
        it != module_registry.end(),
        WaspStage::Compiler,
        "Module not found in workspace: " + abs_path.string()
    );

    return std::distance(module_registry.begin(), it);
}

std::string Workspace::get_module_path(int module_index) const
{
    auto it = module_registry.begin();
    std::advance(it, module_index);
    return it->first.string();
}

} // namespace Wasp
