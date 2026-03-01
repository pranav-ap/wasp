#pragma once

#include "OpCode.h"
#include "ObjectStores.h"
#include "CFGraph.h"

#include <string>
#include <vector>
#include <memory>
#include <map>
#include <iostream>
#include <ostream>

namespace Wasp
{

	class InstructionPrinter
	{
		ConstantPool_ptr constant_pool;
		std::map<int, std::string> name_map;

		std::string stringify_instruction(std::byte opcode, std::byte operand);
		std::string stringify_instruction(std::byte opcode, std::byte operand_1, std::byte operand_2);

	public:
		InstructionPrinter(ConstantPool_ptr constant_pool, std::map<int, std::string> name_map)
			: constant_pool(constant_pool), name_map(name_map) {};

		void print(const CodeObject &code_object, std::ostream &out = std::cout);
		void print(const CFGraph &graph, std::ostream &out = std::cout);
	};

	using InstructionPrinter_ptr = std::shared_ptr<InstructionPrinter>;
}
