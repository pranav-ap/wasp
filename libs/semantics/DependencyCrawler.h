#pragma once

#include "Token.h"
#include "Workspace.h"

#include <filesystem>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace Wasp
{

class DependencyCrawler
{
private:
    std::shared_ptr<Workspace> workspace;
    std::vector<Module_ptr> build_order;

    // Cycle detection
    std::set<std::filesystem::path> currently_visiting;
    std::set<std::filesystem::path> visited;

    void traverse_edges(const std::filesystem::path& file_path);

    std::filesystem::path get_base_path(
        const std::optional<TokenType>& modifier,
        int access_argument,
        const std::filesystem::path& current_file
    );

    std::filesystem::path resolve_gateway(std::filesystem::path base);

    std::filesystem::path resolve_import_path(
        const std::optional<TokenType>& modifier,
        int access_argument,
        const std::vector<std::string>& path_segments,
        const std::filesystem::path& current_file
    );

public:
    explicit DependencyCrawler(std::shared_ptr<Workspace> workspace)
        : workspace(std::move(workspace))
    {
    }

    std::vector<Module_ptr> calculate_build_order(
        const std::filesystem::path& entry_file
    );
};

} // namespace Wasp
