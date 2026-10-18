#include "ShaderPackage.h"
#include <algorithm>
#include <stdexcept>

namespace Gx { namespace Native {
namespace {
void Require(bool valid, const char* message) {
    if (!valid) throw std::runtime_error(message);
}
struct Reader {
    const std::vector<uint8_t>& bytes;
    size_t at = 0;
    void Extent(size_t size) const {
        Require(size <= bytes.size() - at, "Truncated 12340 shader package");
    }
    uint32_t Integer(size_t width) {
        Extent(width);
        uint32_t value = 0;
        for (size_t i = 0; i < width; ++i) value |= uint32_t(bytes[at++]) << (8 * i);
        return value;
    }
};
}
ShaderPackage DecodeShaderPackage(const std::vector<uint8_t>& file, uint32_t permutations) {
    Require(file.size() <= 64 * 1024 * 1024, "Native shader package exceeds 64 MiB limit");
    Require(permutations && permutations <= 65536, "Invalid native shader permutation count");
    Reader reader{file};
    Require(reader.Integer(4) == 0x47585348, "Invalid 12340 shader magic (expected HSXG)");
    Require(reader.Integer(4) == 0x10003, "Unsupported shader version (12340 requires 0x10003)");
    ShaderPackage result;
    result.unknown = reader.Integer(4);
    Require(permutations <= (file.size() - reader.at) / 16, "Truncated shader record table");
    result.permutations.reserve(permutations);
    for (uint32_t i = 0; i < permutations; ++i) {
        ShaderRecord record;
        record.unknown0 = reader.Integer(4);
        record.unknown1 = reader.Integer(4);
        record.unknown2 = static_cast<uint16_t>(reader.Integer(2));
        record.unknown3 = static_cast<uint16_t>(reader.Integer(2));
        uint32_t size = reader.Integer(4);
        Require(size <= 1024 * 1024, "Native shader program exceeds 1 MiB limit");
        reader.Extent(size);
        record.code.assign(file.begin() + reader.at, file.begin() + reader.at + size);
        reader.at += size;
        size_t padding = (4 - (size % 4)) % 4;
        reader.Extent(padding);
        reader.at += padding;
        result.permutations.push_back(std::move(record));
    }
    Require(reader.at == file.size(), "Unexpected trailing shader records/data; check caller permutation count");
    return result;
}
std::string PrepareArbProgram(const ShaderRecord& shader, ShaderStage stage) {
    const auto& code = shader.code;
    Require(!code.empty() && code.size() <= 1024 * 1024 && code.back() == 0,
            "ARB shader requires bounded NUL-terminated code");
    Require(std::find(code.begin(), code.end() - 1, uint8_t(0)) == code.end() - 1,
            "Embedded NUL in ARB shader");
    Require(std::all_of(code.begin(), code.end() - 1, [](uint8_t c) {
        return c == '\n' || c == '\r' || c == '\t' || (c >= 32 && c <= 126);
    }), "Non-ASCII ARB shader");
    Require(stage == ShaderStage::Vertex || stage == ShaderStage::Fragment, "Invalid shader stage");
    std::string source(code.begin(), code.end() - 1);
    const char* header = stage == ShaderStage::Vertex ? "!!ARBvp1.0" : "!!ARBfp1.0";
    Require(source.compare(0, 10, header) == 0, "ARB shader stage/dialect mismatch");
    // Adapt only the program namespace token, leaving comments and unrelated
    // identifiers intact. Unlike the legacy text scan, every extent is bounded.
    bool comment = false;
    for (size_t i = 0; i < source.size(); ++i) {
        if (source[i] == '\n' || source[i] == '\r') comment = false;
        if (source[i] == '#') comment = true;
        if (comment || source.compare(i, 13, "program.local") != 0) continue;
        auto identifier = [](char c) {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                   (c >= '0' && c <= '9') || c == '_' || c == '.';
        };
        if ((i && identifier(source[i - 1])) ||
            (i + 13 < source.size() && identifier(source[i + 13]))) continue;
        source.replace(i + 7, 6, ".env  ");
        i += 12;
    }
    return source;
}
}}
