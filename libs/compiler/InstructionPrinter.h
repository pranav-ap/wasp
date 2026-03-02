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

		std::string stringify_instruction(std::byte opcode, std::byte operand, const std::map<int, std::string> &names);
		std::string stringify_instruction(std::byte opcode, std::byte operand_1, std::byte operand_2, const std::map<int, std::string> &names);

	public:
		InstructionPrinter(ConstantPool_ptr constant_pool)
			: constant_pool(constant_pool) {};

		void print(const CodeObject &code_object, std::ostream &out = std::cout);
		void print(const CFGraph &graph, std::ostream &out = std::cout);
		void print_pool(std::ostream &out);
	};

	using InstructionPrinter_ptr = std::shared_ptr<InstructionPrinter>;
}
