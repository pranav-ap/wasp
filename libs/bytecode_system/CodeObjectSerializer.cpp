#include "CodeObjectSerializer.h"
#include "CFGraph.h"

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <ios>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>

namespace Wasp {

// ==========================================
// Serialization (Writing)
// ==========================================
void CodeObjectSerializer::write_file(const std::string& file_path, const CodeObject& bytecode) {
    std::ofstream file(file_path, std::ios::binary | std::ios::out);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file for writing: " + file_path);
    }

    // Write Header
    file.write(MAGIC_NUMBER, sizeof(MAGIC_NUMBER));
    file.write(reinterpret_cast<const char*>(&VERSION), sizeof(VERSION));

    // Write the CodeObject
    write_code_object(file, bytecode);

    file.close();
}

// ==========================================
// Deserialization (Reading)
// ==========================================
CodeObject CodeObjectSerializer::read_file(const std::string& file_path) {
    std::ifstream file(file_path, std::ios::binary | std::ios::in | std::ios::ate);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file for reading: " + file_path);
    }

    std::streamsize file_size = file.tellg();
    if (file_size < sizeof(MAGIC_NUMBER) + sizeof(VERSION)) {
        throw std::runtime_error("Invalid or corrupted Wasp bytecode file.");
    }

    file.seekg(0, std::ios::beg);

    // Verify Header
    char magic[4];
    file.read(magic, sizeof(magic));
    if (magic[0] != 'W' || magic[1] != 'A' || magic[2] != 'S' || magic[3] != 'P') {
        throw std::runtime_error("Invalid file format: Missing WASP magic number.");
    }

    uint8_t version;
    file.read(reinterpret_cast<char*>(&version), sizeof(version));
    if (version != VERSION) {
        throw std::runtime_error("Unsupported Wasp bytecode version.");
    }

    // Read the CodeObject
    CodeObject deserialized_code = read_code_object(file);

    file.close();
    return deserialized_code;
}

// ==========================================
// Binary Helpers
// ==========================================

void CodeObjectSerializer::write_string(std::ofstream& out, const std::string& str) {
    size_t len = str.size();
    out.write(reinterpret_cast<const char*>(&len), sizeof(len));
    out.write(str.data(), len);
}

std::string CodeObjectSerializer::read_string(std::ifstream& in) {
    size_t len;
    in.read(reinterpret_cast<char*>(&len), sizeof(len));
    std::string str(len, '\0');
    in.read(str.data(), len);
    return str;
}

void CodeObjectSerializer::write_code_object(std::ofstream& out, const CodeObject& code) {
    // Write Name
    write_string(out, code.name);

    // Write Local Names Map
    size_t map_size = code.local_names.size();
    out.write(reinterpret_cast<const char*>(&map_size), sizeof(map_size));
    for (const auto& [id, name] : code.local_names) {
        out.write(reinterpret_cast<const char*>(&id), sizeof(id));
        write_string(out, name);
    }

    // Write Instructions
    size_t inst_size = code.length();
    out.write(reinterpret_cast<const char*>(&inst_size), sizeof(inst_size));
    out.write(reinterpret_cast<const char*>(code.data()), inst_size);
}

CodeObject CodeObjectSerializer::read_code_object(std::ifstream& in) {
    // Read Name
    std::string name = read_string(in);

    // Read Local Names Map
    size_t map_size;
    in.read(reinterpret_cast<char*>(&map_size), sizeof(map_size));
    std::map<int, std::string> local_names;
    for (size_t i = 0; i < map_size; ++i) {
        int id;
        in.read(reinterpret_cast<char*>(&id), sizeof(id));
        local_names[id] = read_string(in);
    }

    // Read Instructions
    size_t inst_size;
    in.read(reinterpret_cast<char*>(&inst_size), sizeof(inst_size));
    ByteVector instrs(inst_size);
    in.read(reinterpret_cast<char*>(instrs.data()), inst_size);

    return CodeObject(std::move(instrs), std::move(local_names), std::move(name));
}

} // namespace Wasp