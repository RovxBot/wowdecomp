#include "AssetBytes.h"
#include <set>

namespace Blizzard { namespace Assets {
using namespace Detail;
Wmo DecodeWmo(const Bytes& file) {
    auto chunks = ReadChunks(View(file));
    Version(chunks, 17);
    Wmo result;
    auto root = chunks.find(Tag("MOHD"));
    if (root != chunks.end()) {
        auto header = Required(chunks, "MOHD", 64);
        result.materials = header.U32(0); result.groups = header.U32(4);
        if (result.groups > 65536 || result.materials > 65536) throw Error("Invalid WMO root counts");
        auto groups = Required(chunks, "MOGI", size_t(result.groups) * 32);
        auto materials = Required(chunks, "MOMT", size_t(result.materials) * 64);
        if (groups.size != size_t(result.groups) * 32 || materials.size != size_t(result.materials) * 64)
            throw Error("WMO root count/extent mismatch");
        for (size_t i = 0; i < result.groups; ++i) {
            groups.Vector(i * 32 + 4); groups.Vector(i * 32 + 16);
        }
        header.Vector(36); header.Vector(48);
        return result;
    }
    auto group = Required(chunks, "MOGP", 68);
    group.Vector(12); group.Vector(24);
    auto geometry = ReadChunks(group.Slice(68, group.size - 68));
    auto positions = Required(geometry, "MOVT", 0), normals = Required(geometry, "MONR", 0);
    auto indices = Required(geometry, "MOVI", 0);
    if (positions.size % 12 || positions.size > 65536 * 12 || normals.size != positions.size || indices.size % 6)
        throw Error("Invalid WMO geometry extents");
    for (size_t i = 0; i < positions.size; i += 12) {
        result.vertices.push_back(positions.Vector(i)); result.normals.push_back(normals.Vector(i));
    }
    for (size_t i = 0; i < indices.size; i += 2) {
        uint16_t index = indices.U16(i);
        if (index >= result.vertices.size()) throw Error("WMO triangle index out of range");
        result.indices.push_back(index);
    }
    auto mopy = Required(geometry, "MOPY", result.indices.size() / 3 * 2);
    if (mopy.size != result.indices.size() / 3 * 2) throw Error("WMO triangle material count mismatch");
    return result;
}
MapDefinition DecodeWdt(const Bytes& file) {
    auto chunks = ReadChunks(View(file)); Version(chunks, 18);
    auto header = Required(chunks, "MPHD", 32), tiles = Required(chunks, "MAIN", 64 * 64 * 8);
    if (tiles.size != 64 * 64 * 8) throw Error("Invalid WDT tile table");
    MapDefinition result; result.flags = header.U32(0);
    for (uint32_t y = 0; y < 64; ++y) for (uint32_t x = 0; x < 64; ++x)
        if (tiles.U32((y * 64 + x) * 8) & 1) result.tiles.emplace_back(x, y);
    return result;
}
std::vector<TerrainCell> DecodeAdt(const Bytes& file) {
    View data(file);
    Chunks singletons;
    std::vector<View> cells;
    for (size_t offset = 0; offset < data.size;) {
        uint32_t tag = data.U32(offset), size = data.U32(offset + 4);
        auto view = data.Slice(offset + 8, size);
        if (tag == Tag("MCNK")) cells.push_back(view);
        else if (!singletons.emplace(tag, view).second) throw Error("Duplicate ADT singleton chunk");
        offset += size_t(size) + 8;
    }
    Version(singletons, 18); Required(singletons, "MHDR", 64);
    if (cells.empty() || cells.size() > 256) throw Error("Invalid ADT cell count");
    std::set<std::pair<uint32_t, uint32_t>> occupied;
    std::vector<TerrainCell> result;
    for (auto cell : cells) {
        cell.Slice(0, 128);
        TerrainCell decoded{};
        decoded.x = cell.U32(4); decoded.y = cell.U32(8); decoded.area = cell.U32(0x34); decoded.holes = cell.U16(0x3c);
        if (decoded.x >= 16 || decoded.y >= 16 || !occupied.emplace(decoded.x, decoded.y).second)
            throw Error("Invalid/duplicate ADT cell coordinate");
        decoded.position = cell.Vector(0x68);
        uint32_t heightOffset = cell.U32(0x14);
        // SMChunk offsets include the surrounding eight-byte MCNK header.
        if (heightOffset < 136 || cell.U32(size_t(heightOffset) - 8) != Tag("MCVT") ||
            cell.U32(size_t(heightOffset) - 4) != 145 * 4) throw Error("Invalid ADT height chunk");
        auto heights = cell.Slice(heightOffset, 145 * 4);
        for (size_t i = 0; i < 145; ++i) {
            decoded.heights[i] = heights.Float(i * 4) + decoded.position.z;
            if (!std::isfinite(decoded.heights[i])) throw Error("Invalid ADT absolute height");
        }
        result.push_back(decoded);
    }
    return result;
}
}}
