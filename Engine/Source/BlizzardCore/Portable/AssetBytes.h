#ifndef WOWDECOMP_NATIVE_ASSET_BYTES_H
#define WOWDECOMP_NATIVE_ASSET_BYTES_H
#include "Assets.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>

namespace Blizzard { namespace Assets { namespace Detail {
struct View {
    const uint8_t* data; size_t size;
    explicit View(const Bytes& b) : data(b.data()), size(b.size()) {}
    View(const uint8_t* p, size_t n) : data(p), size(n) {}
    View Slice(size_t offset, size_t bytes) const {
        if (offset > size || bytes > size - offset) throw Error("Asset extent out of bounds");
        return {data + offset, bytes};
    }
    uint16_t U16(size_t offset) const { auto p = Slice(offset, 2).data; return uint16_t(p[0]) | uint16_t(p[1]) << 8; }
    uint32_t U32(size_t offset) const { return uint32_t(U16(offset)) | uint32_t(U16(offset + 2)) << 16; }
    float Float(size_t offset) const {
        uint32_t bits = U32(offset); float value; std::memcpy(&value, &bits, 4);
        if (!std::isfinite(value)) throw Error("Non-finite asset coordinate");
        return value;
    }
    Vec3 Vector(size_t offset) const { return {Float(offset), Float(offset + 4), Float(offset + 8)}; }
    std::string String(size_t offset) const {
        auto view = Slice(offset, size - std::min(offset, size));
        auto end = static_cast<const uint8_t*>(std::memchr(view.data, 0, view.size));
        if (!end) throw Error("Unterminated asset string");
        return std::string(reinterpret_cast<const char*>(view.data), end - view.data);
    }
};
inline uint32_t Tag(const char* name) {
    return uint32_t(uint8_t(name[0])) << 24 | uint32_t(uint8_t(name[1])) << 16 |
           uint32_t(uint8_t(name[2])) << 8 | uint8_t(name[3]);
}
using Chunks = std::map<uint32_t, View>;
inline Chunks ReadChunks(View view) {
    Chunks chunks;
    for (size_t offset = 0; offset < view.size;) {
        uint32_t tag = view.U32(offset), size = view.U32(offset + 4);
        auto data = view.Slice(offset + 8, size);
        if (!chunks.emplace(tag, data).second) throw Error("Duplicate singleton asset chunk");
        offset += size_t(size) + 8;
    }
    return chunks;
}
inline View Required(const Chunks& chunks, const char* tag, size_t minimum) {
    auto it = chunks.find(Tag(tag));
    if (it == chunks.end() || it->second.size < minimum) throw Error(std::string("Missing/truncated chunk ") + tag);
    return it->second;
}
inline void Version(const Chunks& chunks, uint32_t version) {
    auto v = Required(chunks, "MVER", 4);
    if (v.size != 4 || v.U32(0) != version) throw Error("Unsupported chunked asset version");
}
}}}
#endif
