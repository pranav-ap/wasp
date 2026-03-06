#include "lexer.h"
#include "parser.h"
#include "SemanticAnalyzer.h"
#include "Compiler.h"
#include "VM.h"
#include "Objects.h"
#include "ConstantPool.h"
#include "InstructionPrinter.h"

#include "CLI11.hpp"

#include <iostream>
#include <fstream>
#include <string>
#include <stdexcept>
#include <filesystem>

using std::cout;
using std::endl;
using std::string;

namespace Wasp
{
    string read_file(const string &file_path)
    {
        std::ifstream file(file_path);

        if (!file.is_open())
        {
            throw std::runtime_error("Failed to open file: " + file_path);
        }

        string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();

        return content;
    }

    void log(string file_path, ConstantPool_ptr pool, CodeObject bytecode)
    {
        std::filesystem::path file_path_obj(file_path);
        string filename = file_path_obj.filename().string();

        string log_dir = "/workspaces/wasp/logs/samples";
        string log_file_path = log_dir + "/" + filename + ".debug";
        std::ofstream log_file(log_file_path);

        InstructionPrinter printer(pool);

        if (log_file.is_open())
        {
            printer.print(bytecode, log_file);
            printer.print_pool(log_file);
            log_file.close();
        }
    }

    void run(string file_path)
    {
        string code = read_file(file_path);

        Lexer lexer;
        Parser parser;

        auto tokens = lexer.run(code);
        auto mod = parser.run(tokens);

        SemanticAnalyzer analyzer;
        analyzer.run(mod);

        Compiler compiler;
        auto [pool, bytecode] = compiler.run(mod);

        log(file_path, pool, bytecode);

        auto main_module = std::make_shared<FunctionObject>(std::move(bytecode));

        VM vm(pool);
        vm.run(main_module);
    }
}

int main(int argc, char **argv)
{
    CLI::App app{"Wasp Lang"};

    string file_path;

    app.add_option("file", file_path, "The .wasp file to execute")
        ->required()
        ->check(CLI::ExistingFile);

    CLI11_PARSE(app, argc, argv);

    try
    {
        Wasp::run(file_path);
    }
    catch (const std::exception &e)
    {
        std::cerr << "Fatal Error: " << e.what() << endl;
        return 1;
    }

    return 0;
}
