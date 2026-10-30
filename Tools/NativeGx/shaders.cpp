#include "Engine/Source/Gx/Portable/ShaderPackage.h"
#include <functional>
#include "GlowFixture.h"
#include <iostream>
#include <stdexcept>

using namespace Gx::Native;
using Bytes = std::vector<uint8_t>;
namespace {
void Check(bool valid) { if (!valid) throw std::runtime_error("Shader fixture mismatch"); }
void Reject(const std::function<void()>& operation) {
    try { operation(); } catch (const std::runtime_error&) { return; }
    throw std::runtime_error("Malformed shader was accepted");
}
void U32(Bytes& bytes, uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) bytes.push_back(static_cast<uint8_t>(value >> (8 * i)));
}
Bytes Record(const Bytes& code) {
    Bytes bytes;
    U32(bytes, 0x12345678); U32(bytes, 0x87654321);
    bytes.insert(bytes.end(), {0x34, 0x12, 0xcd, 0xab});
    U32(bytes, static_cast<uint32_t>(code.size()));
    bytes.insert(bytes.end(), code.begin(), code.end());
    while (bytes.size() % 4) bytes.push_back(0xcc);
    return bytes;
}
}
int main() {
    try {
        Bytes bytes{'H', 'S', 'X', 'G'};
        U32(bytes, 0x10003); U32(bytes, 0xfedcba98);
        // All four padding widths, including empty (unavailable) code. The
        // header's opaque word is deliberately not treated as a record count.
        for (Bytes code : {Bytes{}, Bytes{1}, Bytes{2, 3}, Bytes{4, 5, 6}, Bytes{7, 8, 9, 10}}) {
            auto record = Record(code);
            bytes.insert(bytes.end(), record.begin(), record.end());
        }
        auto decoded = DecodeShaderPackage(bytes, 5);
        Check(decoded.unknown == 0xfedcba98 && decoded.permutations.size() == 5);
        for (const auto& shader : decoded.permutations)
            Check(shader.unknown0 == 0x12345678 && shader.unknown1 == 0x87654321 &&
                  shader.unknown2 == 0x1234 && shader.unknown3 == 0xabcd);
        Check(decoded.permutations[4].code == Bytes({7, 8, 9, 10}));
        for (size_t length = 0; length < bytes.size(); ++length) {
            Bytes truncated(bytes.begin(), bytes.begin() + length);
            Reject([&] { DecodeShaderPackage(truncated, 5); });
        }
        Reject([&] { DecodeShaderPackage(bytes, 0); });
        Reject([&] { DecodeShaderPackage(bytes, UINT32_MAX); });
        Reject([&] { DecodeShaderPackage(bytes, 4); });
        Reject([&] { DecodeShaderPackage(bytes, 6); });
        auto bad = bytes; bad[0] = 'G';
        Reject([&] { DecodeShaderPackage(bad, 5); });
        bad = bytes; bad[4] = 4; // 15595 version must not pass as 12340.
        Reject([&] { DecodeShaderPackage(bad, 5); });
        bad = bytes;
        for (int i = 24; i < 28; ++i) bad[i] = 255;
        Reject([&] { DecodeShaderPackage(bad, 5); });
        ShaderRecord shader;
        const std::string source = "!!ARBvp1.0\n# program.local[9]\n"
            "PARAM c[4] = { program.local[0..3] };\nMOV result.position, vertex.position;\nEND\n";
        shader.code.assign(source.begin(), source.end()); shader.code.push_back(0);
        auto prepared = PrepareArbProgram(shader, ShaderStage::Vertex);
        Check(prepared.size() == source.size() && prepared.find("program.env  [0..3]") != std::string::npos &&
              prepared.find("# program.local[9]") != std::string::npos);
        Check(shader.code[shader.code.size() - 1] == 0); // owner remains unchanged
        Reject([&] { PrepareArbProgram(shader, ShaderStage::Fragment); });
        auto valid = shader;
        shader.code.pop_back(); Reject([&] { PrepareArbProgram(shader, ShaderStage::Vertex); });
        shader = valid; shader.code[15] = 0;
        Reject([&] { PrepareArbProgram(shader, ShaderStage::Vertex); });
        shader = valid; shader.code[15] = 255;
        Reject([&] { PrepareArbProgram(shader, ShaderStage::Vertex); });
        shader = {}; Reject([&] { PrepareArbProgram(shader, ShaderStage::Vertex); });
        shader = valid; shader.code[5] = 'f';
        Check(PrepareArbProgram(shader, ShaderStage::Fragment).find("!!ARBfp1.0") == 0);
        Reject([&] { PrepareArbProgram(shader, static_cast<ShaderStage>(42)); });
        const std::string filter = "!!ARBfp1.0\n# texture[3] stays in a comment\nTEMP r;\n"
            "TEX r, fragment.texcoord[3], texture[3], 2D;\n"
            "TEX r, fragment.texcoord[2], texture[2], 2D;\n"
            "TEX r, fragment.texcoord[1], texture[1], 2D;\n"
            "TEX r, fragment.texcoord[0], texture[0], 2D;\nMOV result.color,r;\nEND\n";
        shader.code.assign(filter.begin(),filter.end()); shader.code.push_back(0);
        const auto alias = AliasArbFilterSamplers(shader);
        const std::string aliased(alias.code.begin(),alias.code.end()-1);
        auto expected = filter;
        for (auto token : {"texture[1]", "texture[2]", "texture[3]"}) {
            const auto at = expected.find(token,expected.find("TEMP"));
            expected[at+8] = '0';
        }
        Check(aliased == expected && std::string(shader.code.begin(),shader.code.end()-1) == filter);
        for (auto operand : {"texture[4]", "texture[12]", "texture[x]"}) {
            auto malformed = filter; malformed.replace(malformed.find("texture[3]",malformed.find("TEMP")),10,operand);
            shader.code.assign(malformed.begin(),malformed.end()); shader.code.push_back(0);
            bool rejected = false;
            try { AliasArbFilterSamplers(shader); } catch (const std::invalid_argument&) { rejected = true; }
            Check(rejected);
        }
        const std::string glow = AuthoredGlow;
        shader.code.assign(glow.begin(),glow.end()); shader.code.push_back(0);
        ValidateArbGlow(shader,0); ValidateArbGlow(shader,1);
        Reject([&] { ValidateArbGlow(shader,2); });
        const auto originalGlow = shader.code;
        // A changed equation with the same family/name must not dispatch.
        for (const char* token : {"LRP", "MUL", "primary.z", "primary.w", "texcoord[1]", "color.w, 1"}) {
            shader.code = originalGlow;
            const auto at = glow.find(token); Check(at != std::string::npos);
            shader.code[at] = 'X';
            Reject([&] { ValidateArbGlow(shader,0); });
        }
        shader.code = originalGlow; shader.code.pop_back();
        Reject([&] { ValidateArbGlow(shader,0); });
        std::cout << "12340 shader metadata/permutations/padding/truncation/version/ARB adaptation passed\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
