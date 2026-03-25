#pragma once

#include "CFGraph.h"
#include "Objects.h"
#include "Workspace.h"

#include <cstddef>
#include <iostream>
#include <map>
#include <memory>
#include <ostream>
#include <string>

namespace Wasp
{
class InstructionPrinter
{
    Workspace_ptr ws;

    std::string stringify_instruction(
        std::byte opcode,
        std::byte operand,
        const std::map<int, std::string>& symbol_id_to_name_map,
        const std::map<int, std::string>& upvalue_index_to_id_name_map);

    std::string stringify_instruction(
        std::byte opcode,
        std::byte operand_1,
        std::byte operand_2,
        const std::map<int, std::string>& symbol_id_to_name_map,
        const std::map<int, std::string>& upvalue_index_to_id_name_map);

    void print_bytecode(
        const CodeObject& code,
        const std::map<int, std::string>& symbol_id_to_name_map,
        const std::map<int, std::string>& upvalue_index_to_id_name_map,
        std::ostream& out);

public:
    InstructionPrinter(Workspace_ptr ws) : ws(ws) {};

    void print(const Object_ptr obj, std::ostream& out = std::cout);
    void print(const StaticFunctionObject_ptr func_obj, std::ostream& out = std::cout);
    void print(const CodeObject& code_object, std::ostream& out);
    void print(const CFGraph& graph, std::ostream& out = std::cout);
    void print_pool_functions(std::ostream& out);
};

using InstructionPrinter_ptr = std::shared_ptr<InstructionPrinter>;
} // namespace Wasp
