#include "DependencyCrawler.h"
#include "Doctor.h"
#include "Statement.h"
#include "Token.h"
#include "Workspace.h"

#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace Wasp
{

std::vector<Module_ptr> DependencyCrawler::calculate_build_order(
    const std::filesystem::path& entry_file
)
{
    traverse_edges(entry_file);
    return build_order;
}

void DependencyCrawler::traverse_edges(const std::filesystem::path& file_path)
{
    auto abs_path = std::filesystem::absolute(file_path);

    // Already processed this module
    if (visited.contains(abs_path))
    {
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

    // Look for Top-Level Imports using the new unified Import node
    for (auto& stmt_ptr : mod->stmts)
    {
        if (stmt_ptr->is<Import>())
        {
            auto& import_stmt = stmt_ptr->as<Import>();

            auto full_filepath = resolve_import_path(
                import_stmt.access_modifier,
                import_stmt.access_argument,
                import_stmt.path,
                abs_path
            );

            import_stmt.absolute_path = full_filepath;
            traverse_edges(full_filepath);
        }
    }

    currently_visiting.erase(abs_path);
    visited.insert(abs_path);
    build_order.push_back(mod);
}

std::filesystem::path DependencyCrawler::resolve_import_path(
    const std::optional<TokenType>& access_modifier,
    int access_argument,
    const std::vector<std::string>& path_segments,
    const std::filesystem::path& current_file
)
{
    std::filesystem::path base = get_base_path(
        access_modifier,
        access_argument,
        current_file
    );

    for (const auto& segment : path_segments)
    {
        base /= segment;
    }

    return resolve_gateway(std::move(base));
}

std::filesystem::path DependencyCrawler::get_base_path(
    const std::optional<TokenType>& access_modifier,
    int access_argument,
    const std::filesystem::path& current_file
)
{
    // No access_modifier = Standard Library
    if (!access_modifier.has_value())
    {
        return workspace->libs_path;
    }

    std::filesystem::path base = current_file.parent_path();

    switch (access_modifier.value())
    {
    case TokenType::TOP:
        return workspace->root_path;
    case TokenType::MY:
        return base;
    case TokenType::OUR:
        return base.parent_path();

    case TokenType::UP: {
        for (int i = 0; i < access_argument; ++i)
        {
            base = base.parent_path();
        }
        return base;
    }

    case TokenType::PKG: {
        int found = 0;

        while (base != base.root_path())
        {
            if (std::filesystem::exists(base / "wasp.yaml"))
            {
                if (++found == access_argument)
                {
                    break;
                }
            }
            base = base.parent_path();
        }

        Doctor::get().assert(
            found == access_argument,
            WaspStage::Captain,
            "Could not resolve pkg() boundary."
        );

        return base;
    }

    default:
        return base;
    }
}

std::filesystem::path DependencyCrawler::resolve_gateway(
    std::filesystem::path base
)
{
    if (std::filesystem::is_directory(base))
    {
        if (std::filesystem::exists(base / "exports.wasp"))
        {
            return base / "exports.wasp";
        }
        if (std::filesystem::exists(base / "main.wasp"))
        {
            return base / "main.wasp";
        }

        Doctor::get().fatal(
            WaspStage::Captain,
            "Directory import missing exports.wasp or main.wasp: " +
                base.string()
        );
    }

    base.replace_extension(".wasp");
    return base;
}

} // namespace Wasp
