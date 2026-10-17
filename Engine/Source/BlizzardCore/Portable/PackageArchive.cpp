#include "Package.h"
#include "Engine/Source/BlizzardCore/Source/Packages/Mopaq/MPQEncrypt.h"
#include "ThirdParty/zlib/zlib.h"
#include <algorithm>
#include <cstring>
#include <limits>

namespace Blizzard { namespace Mopaq { namespace Native {
namespace {
uint16_t U16(const uint8_t* p) { return uint16_t(p[0]) | uint16_t(p[1]) << 8; }
uint32_t U32(const uint8_t* p) { return uint32_t(U16(p)) | uint32_t(U16(p + 2)) << 16; }
uint64_t U64(const uint8_t* p) { return U32(p) | uint64_t(U32(p + 4)) << 32; }
void Decrypt(Bytes& bytes, uint32_t key) {
    // Avoid the original reinterpret_cast<uint32_t*> on packed file storage.
    std::vector<uint32_t> words(bytes.size() / 4);
    for (size_t i = 0; i < words.size(); ++i) words[i] = U32(bytes.data() + i * 4);
    System_MPQEncrypt::Decrypt(words.data(), static_cast<uint32_t>(words.size() * 4), key);
    for (size_t i = 0; i < words.size(); ++i)
        for (size_t j = 0; j < 4; ++j) bytes[i * 4 + j] = static_cast<uint8_t>(words[i] >> (j * 8));
}
}
std::string Name(const std::string& input) {
    if (input.empty() || input.size() > 259 || input.find('\0') != std::string::npos) throw Error("Invalid MPQ member name");
    std::string result;
    size_t start = 0;
    for (size_t i = 0; i <= input.size(); ++i) {
        if (i == input.size() || input[i] == '/' || input[i] == '\\') {
            auto part = input.substr(start, i - start);
            if (part.empty() || part == "." || part == "..") throw Error("Invalid MPQ path component");
            if (!result.empty()) result += '\\';
            for (unsigned char c : part) {
                if (c < 32 || c == ':' || c == 127) throw Error("Invalid MPQ member character");
                result += static_cast<char>(System_MPQEncrypt::s_hashChars[c]);
            }
            start = i + 1;
        }
    }
    return result;
}
struct Archive::State {
    struct Hash { uint32_t a, b; uint16_t locale, platform; uint32_t block; };
    struct Block { uint64_t offset; uint32_t compressed, size, flags; };
    std::string path;
    File::StreamRecord* file = nullptr;
    uint64_t length = 0;
    uint32_t sector = 0;
    std::vector<Hash> hashes;
    std::vector<Block> blocks;
    ~State() { if (file) File::Close(file); }
    Bytes ReadAt(uint64_t offset, size_t count) const {
        if (offset > length || count > length - offset || count > 256 * 1024 * 1024)
            throw Error("MPQ extent exceeds archive: " + path);
        Bytes bytes(count);
        size_t done = 0;
        while (done < count) {
            int32_t chunk = static_cast<int32_t>(std::min<size_t>(count - done, 1024 * 1024));
            if (!File::Read(file, bytes.data() + done, static_cast<int64_t>(offset + done), &chunk) || chunk <= 0)
                throw Error("Short MPQ read: " + path);
            done += chunk;
        }
        return bytes;
    }
};
Archive::Archive(const std::string& path) : state(new State) {
    state->path = path;
    if (!File::Native::Open(path.c_str(), 1 | 4, File::Native::Disposition::OpenExisting, state->file))
        throw Error("Cannot open MPQ: " + path);
    File::FileInfo info{};
    if (!File::GetFileInfo(state->file, &info) || info.m_size < 32) throw Error("Invalid MPQ file size");
    state->length = static_cast<uint64_t>(info.m_size);
    auto header = state->ReadAt(0, 32);
    if (U32(header.data()) != 0x1a51504d) throw Error("Missing MPQ header: " + path);
    uint16_t version = U16(header.data() + 12), shift = U16(header.data() + 14);
    uint32_t headerSize = U32(header.data() + 4);
    if (version > 1 || headerSize != (version ? 44u : 32u) || shift > 12)
        throw Error("Unsupported MPQ header/version");
    state->sector = 512u << shift;
    uint64_t hashOffset = U32(header.data() + 16), blockOffset = U32(header.data() + 20), highOffset = 0;
    uint32_t hashes = U32(header.data() + 24), blocks = U32(header.data() + 28);
    if (!hashes || (hashes & (hashes - 1)) || hashes > 1048576 || blocks > 1048576)
        throw Error("Invalid MPQ table counts");
    if (version) {
        auto extension = state->ReadAt(32, 12);
        highOffset = U64(extension.data());
        hashOffset |= uint64_t(U16(extension.data() + 8)) << 32;
        blockOffset |= uint64_t(U16(extension.data() + 10)) << 32;
    }
    if (hashOffset < headerSize || blockOffset < headerSize || (highOffset && highOffset < headerSize))
        throw Error("MPQ tables overlap header");
    auto hashBytes = state->ReadAt(hashOffset, size_t(hashes) * 16);
    auto blockBytes = state->ReadAt(blockOffset, size_t(blocks) * 16);
    Decrypt(hashBytes, System_MPQEncrypt::Hash("(hash table)", 3));
    Decrypt(blockBytes, System_MPQEncrypt::Hash("(block table)", 3));
    Bytes high = highOffset ? state->ReadAt(highOffset, size_t(blocks) * 2) : Bytes();
    for (size_t i = 0; i < hashes; ++i) {
        auto p = hashBytes.data() + i * 16;
        State::Hash entry{U32(p), U32(p + 4), U16(p + 8), U16(p + 10), U32(p + 12)};
        if (entry.block < 0xfffffffe && entry.block >= blocks) throw Error("MPQ hash block index out of range");
        state->hashes.push_back(entry);
    }
    for (size_t i = 0; i < blocks; ++i) {
        auto p = blockBytes.data() + i * 16;
        state->blocks.push_back({U32(p) | (high.empty() ? 0 : uint64_t(U16(high.data() + i * 2)) << 32),
                                 U32(p + 4), U32(p + 8), U32(p + 12)});
    }
}
Archive::~Archive() = default;
const std::string& Archive::Path() const { return state->path; }
std::optional<Bytes> Archive::Read(const std::string& input, bool& deleted, size_t limit) const {
    auto name = Name(input);
    deleted = false;
    uint32_t a = System_MPQEncrypt::Hash(name.c_str(), 1), b = System_MPQEncrypt::Hash(name.c_str(), 2);
    size_t start = System_MPQEncrypt::Hash(name.c_str(), 0) & (state->hashes.size() - 1);
    const State::Block* block = nullptr;
    for (size_t i = 0; i < state->hashes.size(); ++i) {
        const auto& hash = state->hashes[(start + i) & (state->hashes.size() - 1)];
        if (hash.block == 0xffffffff) break;
        // The original mounted retail archives use locale=0, platform=0.
        if (hash.block != 0xfffffffe && hash.a == a && hash.b == b && !hash.locale && !hash.platform) {
            block = &state->blocks[hash.block]; break;
        }
    }
    if (!block) return std::nullopt;
    if (block->flags & 0x02000000) { deleted = true; return std::nullopt; }
    if (!(block->flags & 0x80000000)) throw Error("Inactive MPQ block referenced by hash");
    // Delta patch files are a later format, not original 12340 replacement data.
    if (block->flags & ~(0x80000000u | 0x04000000u | 0x01000000u | 0x00030000u | 0x00000300u))
        throw Error("Unsupported MPQ block flags");
    if ((block->flags & 0x300) == 0x300 || (block->flags & 0x20000 && !(block->flags & 0x10000)))
        throw Error("Invalid MPQ compression/encryption flags");
    if (block->size > limit || block->size > 256 * 1024 * 1024 || block->compressed > 256 * 1024 * 1024)
        throw Error("MPQ member exceeds read limit");
    auto packed = state->ReadAt(block->offset, block->compressed);
    uint32_t key = System_MPQEncrypt::Hash(name.substr(name.find_last_of('\\') + 1).c_str(), 3);
    if (block->flags & 0x20000) key = (key + static_cast<uint32_t>(block->offset)) ^ block->size;
    bool encrypted = (block->flags & 0x10000) != 0, compressed = (block->flags & 0x300) != 0;
    if (!block->size) {
        if (!packed.empty()) throw Error("Nonempty encoding of empty MPQ member");
        return Bytes();
    }
    if (block->flags & 0x01000000) {
        if (encrypted) Decrypt(packed, key);
        auto output = compressed ? Decompress(packed, block->size, block->flags & 0x100) : packed;
        if (output.size() != block->size) throw Error("Short MPQ single-unit output");
        return output;
    }
    size_t sectors = (uint64_t(block->size) + state->sector - 1) / state->sector;
    std::vector<uint32_t> offsets;
    Bytes checksums;
    if (compressed) {
        size_t tableBytes = (sectors + 1) * 4;
        if (packed.size() < tableBytes) throw Error("Truncated MPQ sector table");
        // 12340 detects a checksum table from the first sector offset, even
        // without the advisory sector-CRC flag (IOMpqReadUnit::LoadSectorTable).
        Bytes table(packed.begin(), packed.begin() + std::min(packed.size(), tableBytes + 4));
        if (encrypted) Decrypt(table, key - 1);
        uint32_t first = U32(table.data());
        bool crc = first >= tableBytes + 4;
        if (first != tableBytes && first != tableBytes + 4) throw Error("Invalid MPQ sector table extent");
        size_t entries = sectors + 1 + (crc ? 1 : 0);
        if (table.size() < entries * 4) throw Error("Truncated MPQ checksum offset");
        for (size_t i = 0; i < entries; ++i) {
            uint32_t offset = U32(table.data() + i * 4);
            if (offset > packed.size() || (i && offset < offsets.back()) || (i && i <= sectors && offset == offsets.back()))
                throw Error("MPQ sector offset out of bounds");
            offsets.push_back(offset);
        }
        if (offsets.back() != packed.size()) throw Error("MPQ encoded extent mismatch");
        if (crc && offsets[sectors + 1] > offsets[sectors])
            checksums = Decompress(Bytes(packed.begin() + offsets[sectors], packed.end()), sectors * 4);
        if (!checksums.empty() && checksums.size() != sectors * 4) throw Error("Short MPQ checksum table");
    } else {
        if (packed.size() != block->size) throw Error("Uncompressed MPQ size mismatch");
        for (size_t i = 0; i < sectors; ++i) offsets.push_back(static_cast<uint32_t>(i * state->sector));
        offsets.push_back(block->size);
    }
    Bytes output;
    output.reserve(block->size);
    for (size_t i = 0; i < sectors; ++i) {
        Bytes sector(packed.begin() + offsets[i], packed.begin() + offsets[i + 1]);
        if (encrypted) Decrypt(sector, key + static_cast<uint32_t>(i));
        if (!checksums.empty()) {
            uint32_t expected = U32(checksums.data() + i * 4);
            uint32_t actual = static_cast<uint32_t>(adler32(0, sector.data(), static_cast<uInt>(sector.size())));
            if (!actual) actual = 0xffffffff;
            if (expected && actual != expected) throw Error("MPQ sector checksum failed");
        }
        size_t length = std::min<size_t>(state->sector, block->size - output.size());
        auto bytes = compressed ? Decompress(sector, length, block->flags & 0x100) : sector;
        if (bytes.size() != length) throw Error("Short MPQ sector output");
        output.insert(output.end(), bytes.begin(), bytes.end());
    }
    return output;
}
}}}
