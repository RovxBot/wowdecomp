#include "AssetBytes.h"
#include <new>

namespace Blizzard { namespace Assets {
using namespace Detail;
Dbc::Dbc(const Bytes& file) {
    View data(file);
    if (data.U32(0) != 0x43424457) throw Error("Expected original WDBC data");
    rows = data.U32(4); fields = data.U32(8);
    uint32_t stride = data.U32(12), stringSize = data.U32(16);
    if (!fields || fields > 4096 || rows > 1048576 || uint64_t(fields) * 4 != stride ||
        uint64_t(rows) * stride + stringSize + 20 != file.size() || !stringSize)
        throw Error("Invalid DBC dimensions");
    auto records = data.Slice(20, size_t(rows) * stride);
    values.reserve(size_t(rows) * fields);
    for (size_t offset = 0; offset < records.size; offset += 4) values.push_back(records.U32(offset));
    auto text = data.Slice(20 + records.size, stringSize);
    if (text.data[0] || text.data[text.size - 1]) throw Error("Invalid DBC string block");
    strings.assign(reinterpret_cast<const char*>(text.data), text.size);
}
uint32_t Dbc::UInt(uint32_t row, uint32_t field) const {
    if (row >= rows || field >= fields) throw Error("DBC row/field out of range");
    return values[size_t(row) * fields + field];
}
float Dbc::Float(uint32_t row, uint32_t field) const {
    uint32_t bits = UInt(row, field); float value; std::memcpy(&value, &bits, 4); return value;
}
std::string Dbc::String(uint32_t row, uint32_t field) const {
    auto offset = UInt(row, field);
    if (offset >= strings.size()) throw Error("DBC string offset out of range");
    return strings.c_str() + offset;
}
Model::Model(const Bytes& file) {
    View data(file);
    if (data.U32(0) != 0x3032444d || data.U32(4) != 264 || file.size() < 0x130)
        throw Error("Expected original 12340 MD20 version 264");
    uint32_t nameCount = data.U32(8), nameOffset = data.U32(12);
    if (nameCount) {
        auto text = data.Slice(nameOffset, nameCount);
        name = text.String(0);
    }
    uint32_t count = data.U32(0x3c), offset = data.U32(0x40);
    auto source = data.Slice(offset, uint64_t(count) * 48);
    if (!count || count > 1048576) throw Error("Invalid M2 vertex count");
    // Header and payload are placed in the same bounded reservation, never
    // encoded across unrelated malloc/new allocations.
    arena = std::make_unique<OsNative::ModelArena>(size_t(count) * sizeof(Vertex) + 4096);
    vertices = new (arena->Allocate(sizeof(OsNative::RelativeArray), alignof(OsNative::RelativeArray))) OsNative::RelativeArray{};
    auto output = static_cast<Vertex*>(arena->Allocate(size_t(count) * sizeof(Vertex), alignof(Vertex)));
    for (size_t i = 0; i < count; ++i) {
        auto v = new (output + i) Vertex{};
        v->position = source.Vector(i * 48);
        std::memcpy(v->weights, source.data + i * 48 + 12, 4);
        std::memcpy(v->bones, source.data + i * 48 + 16, 4);
        v->normal = source.Vector(i * 48 + 20);
        for (size_t j = 0; j < 4; ++j) v->uv[j] = source.Float(i * 48 + 32 + j * 4);
    }
    arena->Encode(vertices, output, count, sizeof(Vertex));
}
const Vertex* Model::Vertices() const { return static_cast<const Vertex*>(arena->Resolve(vertices, sizeof(Vertex))); }
uint32_t Model::VertexCount() const { return vertices->count; }
std::vector<uint16_t> DecodeSkin(const Bytes& file, uint32_t modelVertexCount) {
    View data(file);
    if (data.size < 48 || data.U32(0) != 0x4e494b53) throw Error("Expected original SKIN profile");
    uint32_t vertexCount = data.U32(4), indexCount = data.U32(12);
    if (vertexCount > 65536 || indexCount > 4194304 || indexCount % 3) throw Error("Invalid SKIN profile counts");
    auto vertices = data.Slice(data.U32(8), size_t(vertexCount) * 2);
    auto indices = data.Slice(data.U32(16), size_t(indexCount) * 2);
    for (size_t i = 0; i < vertexCount; ++i)
        if (vertices.U16(i * 2) >= modelVertexCount) throw Error("SKIN vertex lookup out of range");
    std::vector<uint16_t> triangles;
    for (size_t i = 0; i < indexCount; ++i) {
        uint16_t index = indices.U16(i * 2);
        if (index >= vertexCount) throw Error("SKIN triangle index out of range");
        triangles.push_back(vertices.U16(size_t(index) * 2));
    }
    return triangles;
}
}}
