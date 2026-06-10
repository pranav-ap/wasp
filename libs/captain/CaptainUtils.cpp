#include "Captain.h"
#include "Doctor.h"
#include "InstructionPrinter.h"
#include "Objects.h"
#include "Workspace.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>

namespace Wasp {
std::string Captain::read_file(const std::filesystem::path& file_path) {
    std::ifstream file(file_path);

    Doctor::get().assert(
        file.is_open(), WaspStage::Captain, "Failed to open file: " + file_path.string()
    );

    return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

void Captain::dump_build_artifacts(
    Workspace_ptr workspace,
    const std::filesystem::path& source_file_path,
    const FunctionBlueprintObject_ptr function_object
)
{
    namespace fs = std::filesystem;

    // "script.wasp" -> "script"
    std::string base_name = source_file_path.stem().string();

    // Ensure build directory exists
    std::error_code ec;
    fs::create_directories(workspace->build_path, ec);

    Doctor::get().assert(
        !ec,
        WaspStage::Captain,
        "Failed to create build directory: " + workspace->build_path.string() +
            " - Error: " + ec.message()
    );

    fs::path debug_log_path = workspace->build_path / (base_name + ".wasp.debug");
    fs::path bytecode_path = workspace->build_path / (base_name + ".wasp.compiled");

    // Store human-readable instructions

    std::ofstream debug_file(debug_log_path);

    Doctor::get().assert(
        debug_file.is_open(),
        WaspStage::Captain,
        "Failed to open debug file for writing: " + debug_log_path.string()
    );

    InstructionPrinter printer(workspace);
    printer.print(function_object, debug_file);
    printer.print_pool_functions(debug_file);
}

} // namespace Wasp
