#include "Workspace.h"
#include "ConstantPool.h"
#include "Doctor.h"
#include "NativeRegistry.h"
#include "Objects.h"

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

const SymbolVector& OverloadsData::get_overloads() const
{
    return overloads;
}

Symbol::Symbol(
    int id,
    std::string name,
    int closure_depth,
    int lexical_depth,
    SymbolPayload payload
)
    : id(id), name(std::move(name)), closure_depth(closure_depth), lexical_depth(lexical_depth),
      payload(std::move(payload)), module_path("")
{
}

bool Symbol::is_global() const
{
    return lexical_depth == 0;
}

bool Symbol::is_exportable() const
{
    bool global = is_global();
    bool is_module = payload_is<ModuleData>();
    bool is_alias = payload_is<SymbolAliasData>();
    return global && !is_module && !is_alias;
}

bool Symbol::is_either_function_or_method() const
{
    return payload_is<FunctionData>() || payload_is<MethodData>();
}

bool Symbol::is_native() const
{
    if (payload_is<FunctionData>())
        return get_payload_as<FunctionData>().is_native;

    if (payload_is<MethodData>())
        return get_payload_as<MethodData>().is_native;

    if (payload_is<OverloadsData>())
    {
        for (const auto& overload : get_payload_as<OverloadsData>().get_overloads())
        {
            if (overload->is_native_function_or_method())
            {
                Doctor::get().assert(
                    get_payload_as<OverloadsData>().get_overloads().size() == 1,
                    WaspStage::Semantics,
                    "Native function overload lists must have exactly one overload"
                );

                return true;
            }
        }
    }

    return false;
}

bool Symbol::is_native_function_or_method() const
{
    if (payload_is<FunctionData>())
        return get_payload_as<FunctionData>().is_native;

    if (payload_is<MethodData>())
        return get_payload_as<MethodData>().is_native;

    return false;
}

bool Symbol::is_generic() const
{
    return payload_is<GenericData>();
}

bool Symbol::should_be_captured(int usage_depth) const
{
    return closure_depth < usage_depth;
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
            [](const MethodData& d)
            {
                return d.type;
            },
            [](const ClassData& d)
            {
                return d.type;
            },
            [](const TraitData& d)
            {
                return d.type;
            },
            [](const ModuleData& d) -> Object_ptr
            {
                return d.mod->type;
            },
            [](const GenericData& d) -> Object_ptr
            {
                return d.type;
            },
            [](const SymbolAliasData& d)
            {
                return d.target->get_type();
            },
            [](const TypeAliasData& d) -> Object_ptr
            {
                return d.type;
            },
            [](const EnumData& d) -> Object_ptr
            {
                return d.type;
            },
            [this](OverloadsData& d) -> Object_ptr
            {
                if (d.type)
                    return d.type;

                ObjectVector overload_types;
                for (const auto& overload : d.get_overloads())
                {
                    overload_types.push_back(overload->get_type());
                }

                d.type = make_object(
                    std::make_shared<ObjectOverloadList>(std::move(overload_types))
                );
                return d.type;
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
            [&](MethodData& d)
            {
                d.type = new_type;
            },
            [&](ClassData& d)
            {
                d.type = new_type;
            },
            [&](TraitData& d)
            {
                d.type = new_type;
            },
            [&](GenericData& d)
            {
                d.type = new_type;
            },
            [&](EnumData& d)
            {
                d.type = new_type;
            },
            [&](OverloadsData& d)
            {
                d.type = new_type;
            },
            [&](ModuleData& d)
            {
                d.mod->type = new_type;
            },
            [&](SymbolAliasData& d)
            {
                d.target->set_type(new_type);
            },
            [&](TypeAliasData& d)
            {
                d.type = new_type;
            }
        },
        payload
    );
}

void Symbol::mark_as_native()
{
    std::visit(
        ::overloaded{
            [&](FunctionData& d)
            {
                d.is_native = true;
            },
            [&](MethodData& d)
            {
                d.is_native = true;
            },
            [](auto&)
            {
                Doctor::get().fatal(
                    WaspStage::Semantics,
                    "Only functions and methods can be marked as native"
                );
            }
        },
        payload
    );
}

Symbol_ptr Symbol::resolve()
{
    if (payload_is<SymbolAliasData>())
    {
        return get_payload_as<SymbolAliasData>().target->resolve();
    }

    return shared_from_this();
}

Symbol_ptr SymbolFactory::create_dummy(
    std::string name,
    Object_ptr type,
    int closure_depth,
    int lexical_depth
)
{
    return std::make_shared<Symbol>(
        symbol_id_counter++,
        std::move(name),
        closure_depth,
        lexical_depth,
        VariableData{std::move(type), false}
    );
}

Symbol_ptr SymbolFactory::create_variable(
    std::string name,
    Object_ptr type,
    bool is_mutable,
    int closure_depth,
    int lexical_depth
)
{
    return std::make_shared<Symbol>(
        symbol_id_counter++,
        std::move(name),
        closure_depth,
        lexical_depth,
        VariableData{std::move(type), is_mutable}
    );
}

Symbol_ptr SymbolFactory::create_function(
    std::string name,
    Object_ptr type,
    bool is_native,
    int closure_depth,
    int lexical_depth
)
{
    auto symbol = std::make_shared<Symbol>(
        symbol_id_counter++,
        std::move(name),
        closure_depth,
        lexical_depth,
        FunctionData(is_native)
    );
    symbol->set_type(std::move(type));
    return symbol;
}

Symbol_ptr SymbolFactory::create_method(
    std::string name,
    Object_ptr type,
    bool is_native,
    int closure_depth,
    int lexical_depth
)
{
    auto symbol = std::make_shared<Symbol>(
        symbol_id_counter++,
        std::move(name),
        closure_depth,
        lexical_depth,
        MethodData(is_native)
    );
    symbol->set_type(std::move(type));
    return symbol;
}

Symbol_ptr SymbolFactory::create_overloads(std::string name, int closure_depth, int lexical_depth)
{
    return std::make_shared<Symbol>(
        symbol_id_counter++,
        std::move(name),
        closure_depth,
        lexical_depth,
        OverloadsData{}
    );
}

Symbol_ptr SymbolFactory::create_class(
    std::string name,
    Object_ptr type,
    int closure_depth,
    int lexical_depth
)
{
    return std::make_shared<Symbol>(
        symbol_id_counter++,
        std::move(name),
        closure_depth,
        lexical_depth,
        ClassData{std::move(type)}
    );
}

Symbol_ptr SymbolFactory::create_trait(
    std::string name,
    Object_ptr type,
    int closure_depth,
    int lexical_depth
)
{
    return std::make_shared<Symbol>(
        symbol_id_counter++,
        std::move(name),
        closure_depth,
        lexical_depth,
        TraitData{std::move(type)}
    );
}

Symbol_ptr SymbolFactory::create_generic(
    std::string name,
    Object_ptr type,
    int closure_depth,
    int lexical_depth
)
{
    return std::make_shared<Symbol>(
        symbol_id_counter++,
        std::move(name),
        closure_depth,
        lexical_depth,
        GenericData{std::move(type)}
    );
}

Symbol_ptr SymbolFactory::create_enum(
    std::string name,
    Object_ptr type,
    int closure_depth,
    int lexical_depth
)
{
    return std::make_shared<Symbol>(
        symbol_id_counter++,
        std::move(name),
        closure_depth,
        lexical_depth,
        EnumData{type}
    );
}

Symbol_ptr SymbolFactory::create_type_alias(
    std::string name,
    Object_ptr type,
    int closure_depth,
    int lexical_depth
)
{
    return std::make_shared<Symbol>(
        symbol_id_counter++,
        std::move(name),
        closure_depth,
        lexical_depth,
        TypeAliasData{type}
    );
}

Symbol_ptr SymbolFactory::create_module(std::string name, Module_ptr mod)
{
    return std::make_shared<Symbol>(
        symbol_id_counter++,
        std::move(name),
        0,
        0,
        ModuleData{std::move(mod)}
    );
}

Symbol_ptr SymbolFactory::create_alias(std::string name, Symbol_ptr target)
{
    return std::make_shared<Symbol>(
        symbol_id_counter++,
        std::move(name),
        0,
        0,
        SymbolAliasData{std::move(target)}
    );
}

std::string Module::get_name() const
{
    return absolute_filepath.stem().string();
}

std::string Module::get_path() const
{
    return absolute_filepath.string();
}

int Module::get_member_index(const std::string& member_name) const
{
    for (size_t i = 0; i < exports.size(); i++)
    {
        if (exports[i]->name == member_name)
            return static_cast<int>(i);
    }

    Doctor::get().fatal(
        WaspStage::Semantics,
        "Member '" + member_name + "' not found in module '" + get_name() + "'"
    );
}

Symbol_ptr Module::get_member(const std::string& member_name) const
{
    for (const auto& sym : exports)
    {
        if (sym->name == member_name)
            return sym;
    }

    Doctor::get().fatal(
        WaspStage::Semantics,
        "Member '" + member_name + "' not found in module '" + get_name() + "'"
    );
}

Workspace::Workspace(std::filesystem::path root)
    : root_path(std::filesystem::absolute(root)), build_path(root_path / "build"),
      libs_path(root_path / "libs"), pool(std::make_shared<ConstantPool>()),
      native_registry(std::make_shared<NativeRegistry>(pool))
{
    std::filesystem::create_directories(build_path);
    std::filesystem::create_directories(libs_path);
}

Module_ptr Workspace::get_module(const std::filesystem::path& path)
{
    auto abs_path = std::filesystem::absolute(path);
    if (module_registry.contains(abs_path))
        return module_registry.at(abs_path);
    return nullptr;
}

Module_ptr Workspace::get_module(int module_index)
{
    auto it = module_registry.begin();
    std::advance(it, module_index);
    return it->second;
}

const std::map<std::filesystem::path, Module_ptr>& Workspace::get_all_modules() const
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
