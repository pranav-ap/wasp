#include "Captain.h"
#include "CFGraph.h"
#include "Compiler.h"
#include "ConstantPool.h"
#include "DependencyCrawler.h"
#include "Doctor.h"
#include "Lexer.h"
#include "NativeRegistry.h"
#include "Parser.h"
#include "SemanticAnalyzer.h"
#include "SymbolHoister.h"
#include "VM.h"
#include "Workspace.h"

#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

template <class... Ts> struct overloaded : Ts... {
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp {
Captain::Captain(const std::filesystem::path& target_path) {
    std::filesystem::path clean_target = std::filesystem::absolute(target_path).lexically_normal();

    std::filesystem::path workspace_root =
        std::filesystem::is_directory(clean_target) ? clean_target : clean_target.parent_path();

    if (workspace_root.empty()) {
        workspace_root = std::filesystem::current_path().lexically_normal();
    }

    workspace = std::make_shared<Workspace>(workspace_root);

    entry_file = std::filesystem::is_regular_file(clean_target)
                     ? clean_target
                     : (workspace_root / "main.wasp").lexically_normal();
}

void Captain::parse_modules() {
    for (const auto& entry : std::filesystem::recursive_directory_iterator(workspace->root_path)) {
        if (entry.is_regular_file() && entry.path().extension() == ".wasp") {
            parse_module(entry.path());
        }
    }
}

void Captain::parse_module(const std::filesystem::path& file_path) {
    auto abs_path = std::filesystem::absolute(file_path).lexically_normal();

    if (workspace->get_module(abs_path) != nullptr) {
        return;
    }

    std::string code = read_file(abs_path);

    Lexer lexer;
    auto tokens = lexer.run(code);

    Parser parser;
    auto block = parser.run(tokens);

    auto module = std::make_shared<Module>();
    module->file_path = abs_path;
    module->block = std::move(block);

    workspace->add_module(abs_path, module);
}

std::vector<Module_ptr> Captain::calculate_build_order() {
    DependencyCrawler crawler(workspace);
    auto build_order = crawler.calculate_build_order(entry_file);
    return build_order;
}

void Captain::hoist_symbols(const std::vector<Module_ptr>& build_order) {
    SymbolHoister hoister(workspace);
    hoister.run(build_order);
}

void Captain::type_check_and_link(const std::vector<Module_ptr>& build_order) {
    SemanticAnalyzer sa(workspace);
    sa.run(build_order);
}

void Captain::compile(const std::vector<Module_ptr>& build_order) {
    for (const auto& module : build_order) {
        bool is_main = (module->file_path == entry_file);

        Compiler compiler(workspace->pool, workspace->native_registry);
        module->code = compiler.run(module->block, is_main);

        dump_build_artifacts(workspace, module->file_path, module->code);
    }
}

std::shared_ptr<Workspace> Captain::build() {
    parse_modules();

    auto build_order = calculate_build_order();
    hoist_symbols(build_order);
    type_check_and_link(build_order);
    compile(build_order);

    return workspace;
}

void Captain::execute() {
    auto main_module = workspace->get_module(entry_file);
    Doctor::get().fatal_if_nullptr(main_module, WaspStage::Captain);

    VM vm(workspace);
    vm.run(main_module->code);
}

} // namespace Wasp
