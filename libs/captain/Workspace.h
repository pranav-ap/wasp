#pragma once

#include "CFGraph.h"
#include "ConstantPool.h"
#include "NativeRegistry.h"
#include "Statement.h"

#include <filesystem>
#include <map>
#include <memory>
#include <string>

namespace Wasp {

struct Module {
    Block statements;

    CFGraph graph;
    CodeObject code;
};

using Module_ptr = std::shared_ptr<Module>;

struct Package {
    std::map<std::string, std::shared_ptr<Package>> sub_packages;
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
};

} // namespace Wasp