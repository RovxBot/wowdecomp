#ifndef WOWDECOMP_NATIVE_SHADER_PACKAGE_H
#define WOWDECOMP_NATIVE_SHADER_PACKAGE_H
#include <cstdint>
#include <string>
#include <vector>

namespace Gx { namespace Native {
enum class ShaderStage { Vertex, Fragment };
struct ShaderRecord {
    // Opaque 12340 metadata, retained without assigning unverified semantics.
    uint32_t unknown0 = 0, unknown1 = 0;
    uint16_t unknown2 = 0, unknown3 = 0;
    std::vector<uint8_t> code;
};
struct ShaderPackage {
    uint32_t unknown = 0;
    std::vector<ShaderRecord> permutations;
};
// Native representation of CGxDevice::IShaderLoad / CGxShader::Load (12340).
// The caller supplies the permutation count, as in the legacy interface. No
// legacy object layout, native-endian read, or 15595 shader format is used.
ShaderPackage DecodeShaderPackage(const std::vector<uint8_t>& file, uint32_t permutations);
// Bounded arbvp1/arbfp1 local-to-env adaptation, matching the legacy constant
// API. This first service retains the original 94-register vertex range; model
// bone-range expansion and arbvp1_cg12/NV/D3D dialects remain separate work.
std::string PrepareArbProgram(const ShaderRecord& shader, ShaderStage stage);
// Preserve every instruction/UV while aliasing literal texture[0..3] operands
// onto sampler zero. The caller must prove all four logical bindings identical.
ShaderRecord AliasArbFilterSamplers(const ShaderRecord& shader);
// Admit only the inspected original Glow equations (2D/RECT) or the authored
// equivalent used by fixtures before selecting the one-sampler composition.
// This fingerprint is semantic dispatch, not archive/signature authentication.
void ValidateArbGlow(const ShaderRecord& shader, uint32_t permutation);
}}
#endif
