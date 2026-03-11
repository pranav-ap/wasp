#pragma once

#include "ConstantPool.h"

#include <fstream>

namespace Wasp {

class ConstantPoolSerializer {
public:
    static void write_to_stream(std::ofstream& out, ConstantPool_ptr pool);
    static void read_from_stream(std::ifstream& in, ConstantPool_ptr pool);
};

} // namespace Wasp