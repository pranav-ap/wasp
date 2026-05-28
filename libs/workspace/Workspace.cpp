#include "Workspace.h"
#include "AST.h"
#include "ConstantPool.h"
#include "Doctor.h"
#include "NativeRegistry.h"
#include "Objects.h"

#include <cstddef>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace Wasp
{

// ============================================================================
// Symbol Payload Constructors
// ============================================================================

FunctionSymbol::FunctionSymbol(Object_ptr type, bool is_native)
    : type(std::move(type)), is_native(is_native), definition(nullptr),
      declaration_scope(nullptr)
{
}

VariableSymbol::VariableSymbol(Object_ptr type, bool is_mutable)
    : type(std::move(type)), is_mutable(is_mutable)
{
}

OopsSymbol::OopsSymbol(Object_ptr type)
    : type(std::move(type)), definition(nullptr), declaration_scope(nullptr)
{
}

// ============================================================================
// OverloadsData Implementation
// ============================================================================

const SymbolVector& OverloadsSymbol::get_overloads() const
{
    return overloads;
}

std::vector<std::pair<Symbol_ptr, int>> OverloadsSymbol::
    get_overloads_with_indices() const
{
    std::vector<std::pair<Symbol_ptr, int>> result;
    result.reserve(overloads.size());
    for (size_t i = 0; i < overloads.size(); ++i)
    {
        result.emplace_back(overloads[i], static_cast<int>(i));
    }
    return result;
}

// ============================================================================
// Symbol Constructor
// ============================================================================

Symbol::Symbol(
    int id,
    std::string name,
    int closure_depth,
    int lexical_depth,
    UnderlyingVariant payload
)
    : name(std::move(name)), id(id), closure_depth(closure_depth),
      lexical_depth(lexical_depth), payload(std::move(payload))
{
}

// ============================================================================
// Symbol Type Management
// ============================================================================

Object_ptr Symbol::get_type() const
{
    if (is<VariableSymbol>())
    {
        return as<VariableSymbol>().type;
    }
    if (is<FunctionSymbol>())
    {
        return as<FunctionSymbol>().type;
    }
    if (is<OopsSymbol>())
    {
        return as<OopsSymbol>().type;
    }
    if (is<TemplateParameterSymbol>())
    {
        return as<TemplateParameterSymbol>().type;
    }
    if (is<EnumSymbol>())
    {
        return as<EnumSymbol>().type;
    }
    if (is<TypeAliasSymbol>())
    {
        return as<TypeAliasSymbol>().type;
    }

    if (is<OverloadsSymbol>())
    {
        auto& overloads_data = as<OverloadsSymbol>();

        ObjectVector overload_types;
        for (const auto& overload : overloads_data.overloads)
        {
            overload_types.push_back(overload->get_type());
        }

        auto pocket_type = std::make_shared<PocketType>(overload_types);
        return make_object(pocket_type);
    }

    Doctor::get().fatal(
        WaspStage::Semantics,
        "Symbol does not have a type attribute : " + name
    );
}

void Symbol::set_type(Object_ptr new_type)
{
    if (is<VariableSymbol>())
    {
        as<VariableSymbol>().type = new_type;
    }
    else if (is<FunctionSymbol>())
    {
        as<FunctionSymbol>().type = new_type;
    }
    else if (is<OopsSymbol>())
    {
        as<OopsSymbol>().type = new_type;
    }
    else if (is<TemplateParameterSymbol>())
    {
        as<TemplateParameterSymbol>().type = new_type;
    }
    else if (is<EnumSymbol>())
    {
        as<EnumSymbol>().type = new_type;
    }
    else if (is<TypeAliasSymbol>())
    {
        as<TypeAliasSymbol>().type = new_type;
    }
}

// ============================================================================
// Symbol Native Handling
// ============================================================================

bool Symbol::is_native() const
{
    return is<FunctionSymbol>() && as<FunctionSymbol>().is_native;
}

void Symbol::mark_as_native()
{
    if (is<FunctionSymbol>())
    {
        as<FunctionSymbol>().is_native = true;
    }
}

// ============================================================================
// Symbol Required Marker
// ============================================================================

void Symbol::mark_as_required()
{
    if (is<FunctionSymbol>())
    {
        as<FunctionSymbol>().required_in_class = true;
    }
}

// ============================================================================
// Symbol Overload Management
// ============================================================================

void Symbol::add_overload(Symbol_ptr overload)
{
    if (is<OverloadsSymbol>())
    {
        as<OverloadsSymbol>().overloads.push_back(overload);
    }
    else
    {
        Doctor::get().fatal(
            WaspStage::Semantics,
            "Cannot add overload to non-overload symbol: " + name
        );
    }
}

SymbolVector Symbol::get_overloads() const
{
    if (is<OverloadsSymbol>())
    {
        return as<OverloadsSymbol>().overloads;
    }
    return {};
}

// ============================================================================
// Symbol Resolution (Placeholder - implement as needed)
// ============================================================================

Symbol_ptr Symbol::resolve()
{
    if (is<SymbolAliasSymbol>())
    {
        return as<SymbolAliasSymbol>().target;
    }
    return shared_from_this();
}

// ============================================================================
// Symbol Other Methods (Placeholders - implement as needed)
// ============================================================================

bool Symbol::is_global() const
{
    // Implementation depends on your scope system
    return lexical_depth == 0;
}

bool Symbol::is_exportable() const
{
    // Implementation depends on your export rules
    return is_global();
}

bool Symbol::should_be_captured(int usage_depth) const
{
    return usage_depth > closure_depth;
}

std::string Symbol::to_string() const
{
    return name + " (id=" + std::to_string(id) + ")";
}

// ============================================================================
// SymbolFactory Static Members
// ============================================================================

int SymbolFactory::symbol_id_counter = 0;

void SymbolFactory::reset_counter()
{
    symbol_id_counter = 0;
}

int SymbolFactory::get_current_id()
{
    return symbol_id_counter;
}

Symbol_ptr SymbolFactory::create_symbol(
    const std::string& name,
    Symbol::UnderlyingVariant&& payload,
    int closure_depth,
    int lexical_depth
)
{
    return std::make_shared<Symbol>(
        symbol_id_counter++,
        name,
        closure_depth,
        lexical_depth,
        std::move(payload)
    );
}

Symbol_ptr SymbolFactory::create_symbol(Symbol::UnderlyingVariant&& payload)
{
    return std::make_shared<Symbol>(
        symbol_id_counter++,
        "",
        0,
        0,
        std::move(payload)
    );
}

Symbol_ptr SymbolFactory::create_variable(
    const std::string& name,
    Object_ptr type,
    bool is_mutable,
    int closure_depth,
    int lexical_depth
)
{
    return create_symbol(
        name,
        VariableSymbol{type, is_mutable},
        closure_depth,
        lexical_depth
    );
}

Symbol_ptr SymbolFactory::create_function(
    const std::string& name,
    Object_ptr type,
    bool is_native,
    int closure_depth,
    int lexical_depth
)
{
    return create_symbol(
        name,
        FunctionSymbol{type, is_native},
        closure_depth,
        lexical_depth
    );
}

Symbol_ptr SymbolFactory::create_overloads(
    const std::string& name,
    int closure_depth,
    int lexical_depth
)
{
    return create_symbol(name, OverloadsSymbol{}, closure_depth, lexical_depth);
}

Symbol_ptr SymbolFactory::create_oops(
    const std::string& name,
    Object_ptr type,
    int closure_depth,
    int lexical_depth
)
{
    return create_symbol(name, OopsSymbol{type}, closure_depth, lexical_depth);
}

Symbol_ptr SymbolFactory::create_template_parameter(
    const std::string& name,
    Object_ptr type,
    int closure_depth,
    int lexical_depth
)
{
    return create_symbol(
        name,
        TemplateParameterSymbol{type},
        closure_depth,
        lexical_depth
    );
}

Symbol_ptr SymbolFactory::create_enum(
    const std::string& name,
    Object_ptr type,
    int closure_depth,
    int lexical_depth
)
{
    return create_symbol(name, EnumSymbol{type}, closure_depth, lexical_depth);
}

Symbol_ptr SymbolFactory::create_type_alias(
    const std::string& name,
    Object_ptr type,
    int closure_depth,
    int lexical_depth
)
{
    return create_symbol(
        name,
        TypeAliasSymbol{type},
        closure_depth,
        lexical_depth
    );
}

Symbol_ptr SymbolFactory::create_symbol_alias(
    const std::string& name,
    Symbol_ptr target,
    int closure_depth,
    int lexical_depth
)
{
    return create_symbol(
        name,
        SymbolAliasSymbol{target},
        closure_depth,
        lexical_depth
    );
}

Symbol_ptr SymbolFactory::create_module(
    const std::string& name,
    Module_ptr mod,
    int closure_depth,
    int lexical_depth
)
{
    return create_symbol(name, ModuleSymbol{mod}, closure_depth, lexical_depth);
}

Symbol_ptr SymbolFactory::create_dummy(
    const std::string& name,
    Object_ptr type,
    int closure_depth,
    int lexical_depth
)
{
    return create_variable(name, type, false, closure_depth, lexical_depth);
}

// ============================================================================
// Module Implementation
// ============================================================================

Module::Module(std::filesystem::path file_path, StatementVector stmts)
    : absolute_filepath(std::move(file_path)), stmts(std::move(stmts))
{
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
    for (size_t i = 0; i < exports.size(); ++i)
    {
        if (exports[i] && exports[i]->name == member_name)
        {
            return static_cast<int>(i);
        }
    }
    return -1;
}

Symbol_ptr Module::get_member(const std::string& member_name) const
{
    int index = get_member_index(member_name);
    return index >= 0 ? exports[index] : nullptr;
}

// ============================================================================
// Workspace Implementation
// ============================================================================

Workspace::Workspace(std::filesystem::path root)
    : root_path(std::move(root)), build_path(root_path / "build"),
      libs_path(root_path / "libs"), pool(std::make_shared<ConstantPool>()),
      native_registry(std::make_shared<NativeRegistry>(pool))
{
}

Module_ptr Workspace::get_module(const std::filesystem::path& path)
{
    auto it = module_registry.find(path);
    if (it != module_registry.end())
    {
        return it->second;
    }
    return nullptr;
}

Module_ptr Workspace::get_module(int module_index)
{
    for (const auto& [path, module] : module_registry)
    {
        if (get_module_index(path) == module_index)
        {
            return module;
        }
    }
    return nullptr;
}

const std::map<std::filesystem::path, Module_ptr>& Workspace::
    get_all_modules() const
{
    return module_registry;
}

void Workspace::add_module(const std::filesystem::path& path, Module_ptr module)
{
    module_registry[path] = module;
}

int Workspace::get_module_index(const std::filesystem::path& path) const
{
    int index = 0;
    for (const auto& [p, _] : module_registry)
    {
        if (p == path)
        {
            return index;
        }
        ++index;
    }
    return -1;
}

std::string Workspace::get_module_path(int module_index) const
{
    int index = 0;
    for (const auto& [path, _] : module_registry)
    {
        if (index == module_index)
        {
            return path.string();
        }
        ++index;
    }
    return "";
}

} // namespace Wasp
