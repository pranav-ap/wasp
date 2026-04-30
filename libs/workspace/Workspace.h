#pragma once

#include "AST.h"
#include "CFGraph.h"
#include "ConstantPool.h"
#include "NativeRegistry.h"
#include "Objects.h"

#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace Wasp
{

struct Symbol;
using Symbol_ptr = std::shared_ptr<Symbol>;
using SymbolVector = std::vector<Symbol_ptr>;
using SymbolStringMap = std::map<std::string, Symbol_ptr>;
using SymbolIntMap = std::map<int, Symbol_ptr>;

struct Module;
using Module_ptr = std::shared_ptr<Module>;

struct Workspace;
using Workspace_ptr = std::shared_ptr<Workspace>;

struct TypedData
{
    Object_ptr type;
    TypedData(Object_ptr type = nullptr) : type(std::move(type))
    {
    }
};

struct PossibleNativeData : public TypedData
{
    bool is_native;
    PossibleNativeData(bool is_native) : TypedData(nullptr), is_native(is_native)
    {
    }
};

struct FunctionData : public PossibleNativeData
{
    FunctionData(bool is_native) : PossibleNativeData(is_native)
    {
    }
};

struct MethodData : public PossibleNativeData
{
    MethodData(bool is_native) : PossibleNativeData(is_native)
    {
    }
};

struct FunctionOverloadsData : public TypedData
{
    SymbolVector siblings;
    SymbolVector parents;

    FunctionOverloadsData()
    {
    }

    SymbolVector get_overloads() const;
};

struct MethodOverloadsData : public TypedData
{
    SymbolVector overloads;

    MethodOverloadsData()
    {
    }

    SymbolVector get_overloads() const;
};

struct VariableData : public TypedData
{
    bool is_mutable;
    VariableData(Object_ptr type, bool is_mutable)
        : TypedData(std::move(type)), is_mutable(is_mutable)
    {
    }
};

struct ClassData : public TypedData
{
    using TypedData::TypedData;
};

struct TraitData : public TypedData
{
    using TypedData::TypedData;
};

struct GenericData : public TypedData
{
    using TypedData::TypedData;
};

struct TemplateData : public TypedData
{
    using TypedData::TypedData;
};

struct SymbolAliasData
{
    Symbol_ptr target;
};

struct TypeAliasData : public TypedData
{
};

struct ModuleData
{
    Module_ptr mod;
};

struct EnumData : public TypedData
{
    using TypedData::TypedData;
};

using SymbolPayload = std::variant<
    VariableData,
    FunctionOverloadsData,
    MethodOverloadsData,
    FunctionData,
    MethodData,
    ModuleData,
    ClassData,
    TraitData,
    GenericData,
    EnumData,
    TemplateData,
    TypeAliasData,
    SymbolAliasData>;

struct Symbol : public std::enable_shared_from_this<Symbol>
{
    std::string name;
    std::filesystem::path module_path;
    int id = -1;

    int closure_depth = 0;
    int lexical_depth = 0;

    SymbolPayload payload;

    Symbol(int id, std::string name, int closure_depth, int lexical_depth, SymbolPayload payload);

    bool is_global() const;
    bool is_exportable() const;
    bool is_either_function_or_method() const;
    bool is_native() const;
    bool is_native_function_or_method() const;
    bool is_generic() const;
    bool is_template() const;

    Object_ptr get_type();
    void set_type(Object_ptr new_type);

    void mark_as_native();

    bool should_be_captured(int usage_depth) const;

    Symbol_ptr resolve();

    template <typename T> bool payload_is() const
    {
        return std::holds_alternative<T>(payload);
    }

    template <typename T> T& get_payload_as()
    {
        return std::get<T>(payload);
    }

    template <typename T> const T& get_payload_as() const
    {
        return std::get<T>(payload);
    }
};

class SymbolFactory
{
private:
    inline static int symbol_id_counter{0};

public:
    static Symbol_ptr create_dummy(
        std::string name,
        Object_ptr type,
        int closure_depth = 0,
        int lexical_depth = 0
    );

    static Symbol_ptr create_variable(
        std::string name,
        Object_ptr type,
        bool is_mutable = false,
        int closure_depth = 0,
        int lexical_depth = 0
    );

    static Symbol_ptr create_function(
        std::string name,
        Object_ptr type,
        bool is_native = false,
        int closure_depth = 0,
        int lexical_depth = 0
    );

    static Symbol_ptr create_method(
        std::string name,
        Object_ptr type,
        bool is_native = false,
        int closure_depth = 0,
        int lexical_depth = 0
    );

    static Symbol_ptr create_function_overloads(
        std::string name,
        int closure_depth = 0,
        int lexical_depth = 0
    );

    static Symbol_ptr create_method_overloads(
        std::string name,
        int closure_depth = 0,
        int lexical_depth = 0
    );

    static Symbol_ptr create_class(
        std::string name,
        Object_ptr type = nullptr,
        int closure_depth = 0,
        int lexical_depth = 0
    );

    static Symbol_ptr create_trait(
        std::string name,
        Object_ptr type = nullptr,
        int closure_depth = 0,
        int lexical_depth = 0
    );

    static Symbol_ptr create_generic(
        std::string name,
        Object_ptr type = nullptr,
        int closure_depth = 0,
        int lexical_depth = 0
    );

    static Symbol_ptr create_template(
        std::string name,
        Object_ptr type = nullptr,
        int closure_depth = 0,
        int lexical_depth = 0
    );

    static Symbol_ptr create_enum(
        std::string name,
        Object_ptr type = nullptr,
        int closure_depth = 0,
        int lexical_depth = 0
    );

    static Symbol_ptr create_type_alias(
        std::string name,
        Object_ptr type = nullptr,
        int closure_depth = 0,
        int lexical_depth = 0
    );

    static Symbol_ptr create_module(std::string name, Module_ptr mod);

    static Symbol_ptr create_alias(std::string name, Symbol_ptr target);
};

struct Module
{
    const std::filesystem::path absolute_filepath;

    StatementVector stmts;
    CFGraph graph;
    FunctionBlueprintObject_ptr blueprint;

    SymbolVector exports;
    Object_ptr type;

    Module() = default;
    Module(std::filesystem::path file_path, StatementVector stmts);

    std::string get_name() const;
    std::string get_path() const;

    int get_member_index(const std::string& member_name) const;
    Symbol_ptr get_member(const std::string& member_name) const;
};

class Workspace
{
private:
    std::map<std::filesystem::path, Module_ptr> module_registry;

public:
    const std::filesystem::path root_path;
    const std::filesystem::path build_path;
    const std::filesystem::path libs_path;

    ConstantPool_ptr pool;
    NativeRegistry_ptr native_registry;

    Workspace(std::filesystem::path root);

    Module_ptr get_module(const std::filesystem::path& path);
    Module_ptr get_module(int module_index);

    const std::map<std::filesystem::path, Module_ptr>& get_all_modules() const;

    void add_module(const std::filesystem::path& path, Module_ptr module);

    int get_module_index(const std::filesystem::path& path) const;
    std::string get_module_path(int module_index) const;
};

} // namespace Wasp
