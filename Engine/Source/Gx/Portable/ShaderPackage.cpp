#include "ShaderPackage.h"
#include "Engine/Source/Storm/Source/MD5.h"
#include <cstring>
#include <algorithm>
#include <cctype>
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
ShaderRecord AliasArbFilterSamplers(const ShaderRecord& shader) {
    // Validate ASCII, terminator and stage before scanning bounded source.
    auto source = PrepareArbProgram(shader, ShaderStage::Fragment);
    bool comment = false;
    uint32_t mask = 0;
    for (size_t i = 0; i < source.size(); ++i) {
        if (source[i] == '\n' || source[i] == '\r') comment = false;
        if (source[i] == '#') comment = true;
        if (comment || source.compare(i, 8, "texture[") != 0) continue;
        if ((i && (std::isalnum(static_cast<unsigned char>(source[i-1])) || source[i-1] == '_')) ||
            i + 10 > source.size() || source[i+8] < '0' || source[i+8] > '3' || source[i+9] != ']')
            throw std::invalid_argument("Unsupported FFX filter sampler operand");
        mask |= 1u << (source[i+8] - '0');
        source[i+8] = '0';
        i += 9;
    }
    if (mask != 15) throw std::invalid_argument("FFX filter requires four literal samplers");
    auto result = shader;
    result.code.assign(source.begin(), source.end());
    result.code.push_back(0);
    return result;
}
void ValidateArbGlow(const ShaderRecord& shader, uint32_t permutation) {
    PrepareArbProgram(shader, ShaderStage::Fragment);
    Require(permutation < 2, "Unsupported Glow permutation");
    // Audited 12340 arbfp1 FFXGlow code including its terminator, not the
    // ps_2_0 fingerprints used by the pinned Metal reference. Preserve bytes
    // in the legacy shader; never select an equation from its name alone.
    const uint8_t original[2][16] = {
        {0x10,0x05,0x92,0xbe,0xa5,0x9d,0xbd,0xaf,0x0f,0x9e,0x89,0xcd,0x5e,0x23,0xad,0x1a},
        {0xdb,0xc5,0xae,0xc3,0x7f,0x78,0xf2,0xa4,0x1d,0x91,0xaf,0xc8,0x5c,0x9f,0x9b,0x4c}
    };
    const uint8_t authored[16] = {0x9a,0x86,0x25,0xff,0x43,0xef,0x56,0x91,0x9d,0x04,0xb3,0xe2,0x2f,0xd5,0xd2,0x06};
    MD5_CTX context;
    MD5Init(&context);
    MD5Update(&context, shader.code.data(), uint32_t(shader.code.size()));
    uint8_t digest[16]; MD5Final(digest, &context);
    Require(!std::memcmp(digest, original[permutation], sizeof(digest)) ||
            !std::memcmp(digest, authored, sizeof(digest)),
            "FFXGlow code has no qualified native composition");
}

}}
