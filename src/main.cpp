#include "CFGraph.h"
#include "CodeObjectSerializer.h"
#include "Compiler.h"
#include "ConstantPool.h"
#include "InstructionPrinter.h"
#include "Lexer.h"
#include "NativeRegistry.h"
#include "Objects.h"
#include "Parser.h"
#include "SemanticAnalyzer.h"
#include "VM.h"
#include "Workspace.h"

#include "CLI11.hpp"

#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

using std::string;

namespace Wasp {
string read_file(const string& file_path) {
    std::ifstream file(file_path);

    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + file_path);
    }

    string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    return content;
}

void dump_build_artifacts(
    const string& source_file_path, ConstantPool_ptr pool, const CodeObject& bytecode
) {
    namespace fs = std::filesystem;

    fs::path source_path(source_file_path);

    // "script.wasp" -> "script"
    string base_name = source_path.stem().string();

    fs::path build_dir = fs::path("/workspaces/wasp/workspace") / "build";
    fs::create_directories(build_dir);

    fs::path debug_log_path = build_dir / (base_name + ".wasp.debug");
    fs::path bytecode_path = build_dir / (base_name + ".wasp.compiled");

    // Serialize the bytecode to disk
    Wasp::CodeObjectSerializer::write_file(bytecode_path.string(), bytecode);

    // Store human-readable instructions
    std::ofstream debug_file(debug_log_path);
    if (!debug_file.is_open()) {
        throw std::runtime_error(
            "Failed to open debug file for writing: " + debug_log_path.string()
        );
    }

    InstructionPrinter printer(pool);
    printer.print(bytecode, debug_file);
    printer.print_pool(debug_file);
}

void run(string file_path) {
    string code = read_file(file_path);

    Lexer lexer;
    auto tokens = lexer.run(code);

    Parser parser;
    auto block = parser.run(tokens);

    auto pool = std::make_shared<ConstantPool>();
    auto native_registry = std::make_shared<NativeRegistry>(pool);

    SemanticAnalyzer semantic_analyzer(native_registry);
    semantic_analyzer.run(block);

    Compiler compiler(pool, native_registry);
    auto bytecode = compiler.run(block);

    dump_build_artifacts(file_path, pool, bytecode);

    auto main_module = std::make_shared<FunctionObject>(std::move(bytecode));

    VM vm(pool, native_registry);
    vm.run(main_module);
}
} // namespace Wasp

int main(int argc, char** argv) {
    CLI::App app{"Wasp Lang"};

    string file_path;

    CLI::App* run_cmd = app.add_subcommand("run", "Execute a .wasp file");

    run_cmd->add_option("file", file_path, "The .wasp file to execute")
        ->required()
        ->check(CLI::ExistingFile);

    // Force the user to provide exactly one subcommand
    app.require_subcommand(1);

    CLI11_PARSE(app, argc, argv);

    try {
        if (run_cmd->parsed()) {
            Wasp::run(file_path);
        }
    } catch (const std::exception& e) {
        std::cerr << "Fatal Error: " << e.what() << '\n';
        return 1;
    }

    return 0;
}
