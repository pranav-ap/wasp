#include "Captain.h"
#include "DependencyCrawler.h"
#include "Doctor.h"
#include "Lexer.h"
#include "Parser.h"
#include "Salter.h"
#include "SemanticsAnalyzer.h"
#include "Statement.h"
#include "Token.h"
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

Captain::Captain(const std::filesystem::path& wasp_file_path)
{
    Doctor::captain().check(
        std::filesystem::is_regular_file(wasp_file_path),
        "Wasp file does not exist: " + wasp_file_path.string()
    );

    entry_wasp_file_path = std::filesystem::absolute(wasp_file_path).lexically_normal();

    std::filesystem::path workspace_root = entry_wasp_file_path.parent_path();
    workspace = std::make_shared<Workspace>(workspace_root);
}

void Captain::parse_modules()
{
    Doctor::parser().start();

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

    Doctor::parser().stop();
}

void Captain::parse_module(const std::filesystem::path& file_path)
{
    auto abs_path = std::filesystem::absolute(file_path).lexically_normal();

    if (workspace->module_registry.contains(abs_path))
    {
        return;
    }

    std::string code = read_file(abs_path);

    Lexer lexer;
    std::vector<Token> tokens = lexer.run(code);

    Parser parser;
    Block stmts = parser.run(tokens);

    Module_ptr mod = std::make_shared<Module>(abs_path, stmts);
    mod->save_ast("parser");

    workspace->add_module(abs_path, mod);
}

void Captain::build()
{
    parse_modules();

    DependencyCrawler crawler(workspace);
    std::vector<Module_ptr> build_order = crawler.calculate_build_order(entry_wasp_file_path);

    SemanticsAnalyzer semantics_analyzer(workspace);
    semantics_analyzer.run(build_order);

    Salter salter;
    salter.run(build_order);
}

void Captain::execute()
{
    auto main_module = workspace->get_module(entry_wasp_file_path);
}

} // namespace Wasp
