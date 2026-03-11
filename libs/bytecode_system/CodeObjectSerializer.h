#pragma once

#include "CFGraph.h"
#include <cstdint>
#include <fstream>
#include <string>

namespace Wasp {

class CodeObjectSerializer {
public:
    static constexpr char MAGIC_NUMBER[4] = {'W', 'A', 'S', 'P'};
    static constexpr uint8_t VERSION = 1;

    // ==========================================
    // Serialization (Writing)
    // ==========================================
    static void write_file(const std::string& file_path, const CodeObject& bytecode);

    // ==========================================
    // Deserialization (Reading)
    // ==========================================
    static CodeObject read_file(const std::string& file_path);

private:
    static void write_string(std::ofstream& out, const std::string& str);
    static std::string read_string(std::ifstream& in);

    static void write_code_object(std::ofstream& out, const CodeObject& code);
    static CodeObject read_code_object(std::ifstream& in);
};

} // namespace Wasp