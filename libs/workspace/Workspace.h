#pragma once

#include "AST.h"
#include "CFGraph.h"
#include "ConstantPool.h"
#include "NativeRegistry.h"
#include "Objects.h"
#include "Symbol.h"

#include <filesystem>
#include <map>
#include <memory>
#include <string>

namespace Wasp {

struct Module {
    const std::filesystem::path file_path;

    StatementVector stmts;
    CFGraph graph;
    CodeObject code;

    std::map<std::string, Symbol_ptr> exports;
    Object_ptr type;

    Module() = default;

    Module(std::filesystem::path file_path, StatementVector stmts)
        : file_path(file_path), stmts(stmts) {}

    std::string get_name() const { return file_path.stem().string(); }
};

using Module_ptr = std::shared_ptr<Module>;

struct Package;
using Package_ptr = std::shared_ptr<Package>;

struct Package {
    std::map<std::string, Package_ptr> sub_packages;
    std::map<std::string, Module_ptr> modules;
};

class Workspace {
private:
    // Flat View
    std::map<std::filesystem::path, Module_ptr> module_registry;

    // Hierarchial View
    Package root_package;

public:
    const std::filesystem::path root_path;
    const std::filesystem::path build_path;
    const std::filesystem::path libs_path;

    ConstantPool_ptr pool;
    NativeRegistry_ptr native_registry;

    Workspace(std::filesystem::path root)
        : root_path(std::filesystem::absolute(root)), build_path(root_path / "build"),
          libs_path(root_path / "libs"), pool(std::make_shared<ConstantPool>()),
          native_registry(std::make_shared<NativeRegistry>(pool)) {
        std::filesystem::create_directories(build_path);
        std::filesystem::create_directories(libs_path);
    }

    Module_ptr get_module(const std::filesystem::path& path) {
        auto it = module_registry.find(std::filesystem::absolute(path));
        return (it != module_registry.end()) ? it->second : nullptr;
    }

    const std::map<std::filesystem::path, Module_ptr>& get_all_modules() const {
        return module_registry;
    }

    void add_module(const std::filesystem::path& path, Module_ptr module) {
        module_registry[std::filesystem::absolute(path)] = module;
    }
};

using Workspace_ptr = std::shared_ptr<Workspace>;

} // namespace Wasp
