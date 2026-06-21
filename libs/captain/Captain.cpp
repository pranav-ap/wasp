#include "Captain.h"
#include "Compiler.h"
#include "DependencyCrawler.h"
#include "Doctor.h"
#include "Lexer.h"
#include "Parser.h"
#include "Salter.h"
#include "SemanticsAnalyzer.h"
#include "Workspace.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

namespace
{

std::string read_file(const std::filesystem::path& file_path)
{
    std::ifstream file(file_path);

    Doctor::captain().check(
        file.is_open(),
        "Failed to open file: " + file_path.string()
    );

    return std::string(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>()
    );
}

} // namespace

Captain::Captain(const std::filesystem::path& target_path)
{
    std::filesystem::path clean_target = std::filesystem::absolute(target_path).lexically_normal();

    std::filesystem::path workspace_root = std::filesystem::is_directory(clean_target)
                                               ? clean_target
                                               : clean_target.parent_path();

    if (workspace_root.empty())
    {
        workspace_root = std::filesystem::current_path().lexically_normal();
    }

    workspace = std::make_shared<Workspace>(workspace_root);

    entry_file = std::filesystem::is_regular_file(clean_target)
                     ? clean_target
                     : (workspace_root / "main.wasp").lexically_normal();
}

void Captain::parse_modules()
{
    auto it = std::filesystem::recursive_directory_iterator(workspace->root_path);
    auto end = std::filesystem::recursive_directory_iterator();

    for (; it != end; ++it)
    {
        if (it->is_directory() && it->path().filename() == "frozen")
        {
            it.disable_recursion_pending();
            continue;
        }

        if (it->is_regular_file() && it->path().extension() == ".wasp")
        {
            parse_module(it->path());
        }
    }
}

void Captain::parse_module(const std::filesystem::path& file_path)
{
    auto abs_path = std::filesystem::absolute(file_path).lexically_normal();

    if (workspace->get_module(abs_path) != nullptr)
    {
        return;
    }

    std::string code = read_file(abs_path);

    Lexer lexer;
    auto tokens = lexer.run(code);

    Parser parser;
    auto stmts = parser.run(tokens);

    auto mod = std::make_shared<Module>(abs_path, stmts);
    workspace->add_module(abs_path, mod);
    mod->save_ast("parser");
}

std::vector<Module_ptr> Captain::calculate_build_order()
{
    DependencyCrawler crawler(workspace);
    auto build_order = crawler.calculate_build_order(entry_file);
    return build_order;
}

void Captain::build()
{
    parse_modules();

    auto build_order = calculate_build_order();

    SemanticsAnalyzer semantics_analyzer;
    semantics_analyzer.run(build_order);

    Salter salter;
    salter.run(build_order);

    Compiler compiler;
    compiler.run(build_order);
}

void Captain::execute()
{
    auto main_module = workspace->get_module(entry_file);
    Doctor::captain().fatal_if_nullptr(main_module);
}

} // namespace Wasp
