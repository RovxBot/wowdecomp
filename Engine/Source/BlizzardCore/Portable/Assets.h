#ifndef WOWDECOMP_NATIVE_ASSETS_H
#define WOWDECOMP_NATIVE_ASSETS_H
#include "Package.h"
#include "Engine/Source/Os/Portable/ModelArena.h"

// Bounded scalar inspection decoders. Rendering, animation evaluation, material
// creation and streaming into the legacy scene ABI remain separate work.
namespace Blizzard { namespace Assets {
using Mopaq::Native::Bytes;
using Mopaq::Native::Error;
struct Vec3 { float x, y, z; };
class Dbc {
public:
    explicit Dbc(const Bytes& file);
    uint32_t Rows() const { return rows; }
    uint32_t Fields() const { return fields; }
    uint32_t UInt(uint32_t row, uint32_t field) const;
    float Float(uint32_t row, uint32_t field) const;
    std::string String(uint32_t row, uint32_t field) const;
private:
    uint32_t rows = 0, fields = 0;
    std::vector<uint32_t> values;
    std::string strings;
};
struct Image { uint32_t width, height; Bytes rgba; };
std::vector<Image> DecodeBlp(const Bytes& file);
struct Vertex { Vec3 position; uint8_t weights[4], bones[4]; Vec3 normal; float uv[4]; };
class Model {
public:
    explicit Model(const Bytes& file);
    const Vertex* Vertices() const;
    uint32_t VertexCount() const;
    const std::string& Name() const { return name; }
private:
    std::unique_ptr<OsNative::ModelArena> arena;
    OsNative::RelativeArray* vertices = nullptr;
    std::string name;
};
struct Wmo {
    uint32_t groups = 0, materials = 0;
    std::vector<Vec3> vertices, normals;
    std::vector<uint16_t> indices;
};
Wmo DecodeWmo(const Bytes& file);
struct MapDefinition { uint32_t flags = 0; std::vector<std::pair<uint32_t, uint32_t>> tiles; };
MapDefinition DecodeWdt(const Bytes& file);
struct TerrainCell { uint32_t x, y, area, holes; Vec3 position; std::array<float, 145> heights; };
std::vector<TerrainCell> DecodeAdt(const Bytes& file);
}}
#endif
