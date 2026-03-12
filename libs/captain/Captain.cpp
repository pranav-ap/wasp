#include "Captain.h"
#include "CFGraph.h"
#include "Compiler.h"
#include "ConstantPool.h"
#include "Doctor.h"
#include "InstructionPrinter.h"
#include "Lexer.h"
#include "NativeRegistry.h"
#include "Parser.h"
#include "SemanticAnalyzer.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <utility>

template <class... Ts> struct overloaded : Ts... {
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp {
std::string read_file(const std::string& file_path) {
    std::ifstream file(file_path);

    Doctor::get().assert_true(
        file.is_open(), WaspStage::Captain, "Failed to open file: " + file_path
    );

    return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

void dump_build_artifacts(
    std::shared_ptr<Workspace> workspace,
    const std::filesystem::path& source_file_path,
    const CodeObject& bytecode
) {
    namespace fs = std::filesystem;

    // "script.wasp" -> "script"
    std::string base_name = source_file_path.stem().string();

    fs::path debug_log_path = workspace->build_path / (base_name + ".wasp.debug");
    fs::path bytecode_path = workspace->build_path / (base_name + ".wasp.compiled");

    // Store human-readable instructions

    std::ofstream debug_file(debug_log_path);

    Doctor::get().assert_true(
        debug_file.is_open(),
        WaspStage::Captain,
        "Failed to open debug file for writing: " + debug_log_path.string()
    );

    InstructionPrinter printer(workspace->pool);
    printer.print(bytecode, debug_file);
    printer.print_pool(debug_file);
}

Captain::Captain(const std::filesystem::path& workspace_root)
    : workspace(std::make_shared<Workspace>(workspace_root)) {}

void Captain::parse_workspace(const std::filesystem::path& file_path) {
    auto abs_path = std::filesystem::absolute(file_path);

    if (workspace->get_module(abs_path) != nullptr) {
        return;
    }

    Doctor::get().assert_true(
        !currently_parsing.contains(abs_path),
        WaspStage::Captain,
        "Strict cyclic import detected involving : " + abs_path.string()
    );

    currently_parsing.insert(abs_path);

    // Parse

    std::string code = read_file(abs_path);

    Lexer lexer;
    auto tokens = lexer.run(code);

    Parser parser;
    auto block = parser.run(tokens);

    auto module = std::make_shared<Module>();
    module->block = std::move(block);

    workspace->add_module(abs_path, module);

    auto semantic_analyzer = Wasp::SemanticAnalyzer(workspace->native_registry);
    semantic_analyzer.run(module->block);
}

void Captain::hoist_symbols() {};

void Captain::type_check_and_link() {

};

void Captain::compile() {
    Compiler compiler(workspace->pool, workspace->native_registry);

    for (const auto& [path, module] : workspace->get_all_modules()) {
        module->code = compiler.run(module->block);

        dump_build_artifacts(workspace, path, module->code);
    }
};

std::shared_ptr<Workspace> Captain::build() {
    for (const auto& entry : std::filesystem::recursive_directory_iterator(workspace->root_path)) {
        if (entry.is_regular_file() && entry.path().extension() == ".wasp") {
            parse_workspace(entry.path());
        }
    }

    hoist_symbols();
    type_check_and_link();
    compile();

    return workspace;
}

} // namespace Wasp