#include "DependencyCrawler.h"
#include "Doctor.h"
#include "Statement.h"
#include "Token.h"
#include "Workspace.h"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

template <class... Ts> struct overloaded : Ts... {
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp {

std::vector<Module_ptr>
DependencyCrawler::calculate_build_order(const std::filesystem::path& entry_file) {
    build_order.clear();
    currently_visiting.clear();
    visited.clear();

    traverse_edges(entry_file);

    return build_order;
}

void DependencyCrawler::traverse_edges(const std::filesystem::path& file_path) {
    auto abs_path = std::filesystem::absolute(file_path);

    // Already processes this module
    if (visited.contains(abs_path)) {
        return;
    }

    Doctor::get().assert(
        !currently_visiting.contains(abs_path),
        WaspStage::Captain,
        "Strict cyclic import detected involving: " + abs_path.string()
    );

    auto mod = workspace->get_module(abs_path);
    Doctor::get().fatal_if_nullptr(mod, WaspStage::Captain);

    currently_visiting.insert(abs_path);

    // Look for Top-Level Imports

    for (auto& stmt_ptr : mod->block) {
        if (stmt_ptr->is<SimpleImport>()) {
            auto& import_stmt = stmt_ptr->as<SimpleImport>();

            import_stmt.resolved_path =
                resolve_import_path(import_stmt.access_token_type, import_stmt.path, abs_path);

            traverse_edges(import_stmt.resolved_path);
        } else if (stmt_ptr->is<FromImport>()) {
            auto& import_stmt = stmt_ptr->as<FromImport>();

            import_stmt.resolved_path =
                resolve_import_path(import_stmt.access_token_type, import_stmt.path, abs_path);

            traverse_edges(import_stmt.resolved_path);
        }
    }

    currently_visiting.erase(abs_path);
    visited.insert(abs_path);
    build_order.push_back(mod);
}

std::filesystem::path DependencyCrawler::resolve_import_path(
    const std::optional<TokenType>& access_token_modifier,
    const std::vector<std::string>& path_segments,
    const std::filesystem::path& current_file
) {
    size_t start_idx = 0;

    std::filesystem::path base =
        get_base_path(access_token_modifier, path_segments, current_file, start_idx);

    for (size_t i = start_idx; i < path_segments.size(); ++i) {
        base /= path_segments[i];
    }

    return resolve_gateway(std::move(base));
}

std::filesystem::path DependencyCrawler::get_base_path(
    const std::optional<TokenType>& access_token_modifier,
    const std::vector<std::string>& path_segments,
    const std::filesystem::path& current_file,
    size_t& out_start_idx
) {
    out_start_idx = 0;

    // No access_token_modifier = Standard Library
    if (!access_token_modifier.has_value()) {
        return workspace->libs_path;
    }

    std::filesystem::path base = current_file.parent_path();

    switch (access_token_modifier.value()) {
    case TokenType::TOP:
        return workspace->root_path;
    case TokenType::MY:
        return base;
    case TokenType::OUR:
        return base.parent_path();

    case TokenType::UP: {
        int depth = std::stoi(path_segments[0]);
        for (int i = 0; i < depth; ++i)
            base = base.parent_path();

        // Skip the depth number in the path array
        out_start_idx = 1;

        return base;
    }

    case TokenType::PKG: {
        int target_depth = std::stoi(path_segments[0]);
        int found = 0;

        while (base != base.root_path()) {
            if (std::filesystem::exists(base / "wasp.yaml")) {
                if (++found == target_depth)
                    break;
            }
            base = base.parent_path();
        }

        Doctor::get().assert(
            found == target_depth, WaspStage::Captain, "Could not resolve pkg() boundary."
        );

        out_start_idx = 1;
        return base;
    }

    default:
        return base;
    }
}

std::filesystem::path DependencyCrawler::resolve_gateway(std::filesystem::path base) {
    if (std::filesystem::is_directory(base)) {
        if (std::filesystem::exists(base / "exports.wasp"))
            return base / "exports.wasp";
        if (std::filesystem::exists(base / "main.wasp"))
            return base / "main.wasp";

        Doctor::get().fatal(
            WaspStage::Captain,
            "Directory import missing exports.wasp or main.wasp: " + base.string()
        );
    }

    base.replace_extension(".wasp");
    return base;
}

} // namespace Wasp