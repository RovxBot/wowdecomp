#include "Package.h"
#include "Engine/Source/Storm/Source/MD5.h"
#include "Engine/Source/Storm/Source/Sha1.h"
#include <algorithm>
#include <cstring>

namespace Blizzard { namespace Mopaq { namespace Native {
namespace {
// Public signature verification only, not secret-key cryptography. One spare
// limb retains carry; every intermediate is reduced to [0, modulus).
using Integer = std::vector<uint32_t>;
bool Less(const Integer& a, const Integer& b) {
    for (size_t i = a.size(); i--;) if (a[i] != b[i]) return a[i] < b[i];
    return false;
}
Integer Decode(const uint8_t* bytes, size_t size) {
    Integer value(size / 4 + 1);
    for (size_t i = 0; i < size; ++i) value[i / 4] |= uint32_t(bytes[i]) << (8 * (i % 4));
    return value;
}
Integer Add(const Integer& a, const Integer& b, const Integer& modulus) {
    Integer value(a.size());
    uint64_t carry = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        carry += uint64_t(a[i]) + b[i]; value[i] = static_cast<uint32_t>(carry); carry >>= 32;
    }
    if (!Less(value, modulus)) {
        uint64_t borrow = 0;
        for (size_t i = 0; i < a.size(); ++i) {
            uint64_t sub = uint64_t(modulus[i]) + borrow;
            borrow = uint64_t(value[i]) < sub;
            value[i] = static_cast<uint32_t>(uint64_t(value[i]) - sub);
        }
    }
    return value;
}
Integer Multiply(Integer a, const Integer& b, const Integer& modulus) {
    Integer value(a.size());
    for (size_t i = 0; i < b.size() - 1; ++i) {
        for (unsigned bit = 0; bit < 32; ++bit) {
            if (b[i] & (uint32_t(1) << bit)) value = Add(value, a, modulus);
            a = Add(a, a, modulus);
        }
    }
    return value;
}
unsigned Hex(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    throw Error("Invalid signaturefile MD5 digit");
}
}
std::array<uint8_t, 16> Digest(const Bytes& data) {
    MD5_CTX md5; MD5Init(&md5);
    MD5Update(&md5, data.data(), static_cast<uint32_t>(data.size()));
    std::array<uint8_t, 16> result{}; MD5Final(result.data(), &md5); return result;
}
bool VerifySignature(const Bytes& file, const Bytes& modulusBytes) {
    size_t size = modulusBytes.size();
    if (size < 32 || size > 256 || size % 4 || file.size() < size + 4 || !(modulusBytes[0] & 1)) return false;
    size_t payload = file.size() - size - 4;
    if (std::memcmp(file.data() + payload, "NGIS", 4)) return false;
    auto modulus = Decode(modulusBytes.data(), size);
    auto signature = Decode(file.data() + payload + 4, size);
    if (!Less(signature, modulus)) return false;
    Integer value = signature;
    for (unsigned i = 0; i < 16; ++i) value = Multiply(value, value, modulus);
    value = Multiply(value, signature, modulus); // 2^16 + 1
    Bytes expected(size, 0xbb); expected.back() = 0x0b;
    Sha1 sha; sha.Initialize();
    size_t consumed = 0;
    while (consumed < payload) {
        uint32_t chunk = static_cast<uint32_t>(std::min<size_t>(payload - consumed, 1024 * 1024));
        sha.Append(file.data() + consumed, chunk); consumed += chunk;
    }
    sha.Finalize(expected.data());
    for (size_t i = 0; i < size; ++i)
        if (static_cast<uint8_t>(value[i / 4] >> (8 * (i % 4))) != expected[i]) return false;
    return true;
}
void ReadSignatureRecords(const Bytes& file, const std::string& locale, bool expansion,
                         std::unordered_map<std::string, std::array<uint8_t, 16>>& records) {
    // Production calls this only after the complete original-key RSA check.
    if (file.size() < 260 || std::memcmp(file.data() + file.size() - 260, "NGIS", 4))
        throw Error("Missing signaturefile footer");
    std::string text(file.begin(), file.end() - 260);
    if (text.compare(0, 15, "2\r\n2\r\nRELEASE\r\n")) throw Error("Unsupported signaturefile version/scope");
    std::unordered_map<std::string, std::array<uint8_t, 16>> parsed;
    for (size_t pos = 15; pos < text.size();) {
        size_t end = text.find("\r\n", pos);
        if (end == std::string::npos) throw Error("Truncated signaturefile record");
        std::string line = text.substr(pos, end - pos); pos = end + 2;
        if (line.size() < 41 || line[4] != ';' || line[6] != ';' || line[39] != ';' ||
            (line[5] != 'c' && line[5] != 'e')) throw Error("Invalid signaturefile record");
        std::array<uint8_t, 16> digest{};
        // Client.cpp constructs MD5 from four printed uint32 words, then
        // MD5::ToArray copies those words in little-endian order. The manifest
        // is NOT the conventional byte-ordered hexadecimal digest.
        for (size_t i = 0; i < 16; ++i)
            digest[(i & ~size_t(3)) + 3 - i % 4] = static_cast<uint8_t>(Hex(line[7 + i * 2]) * 16 + Hex(line[8 + i * 2]));
        auto name = Name(line.substr(40));
        if ((line.compare(0, 4, "base") == 0 || line.compare(0, 4, locale) == 0) && (line[5] == 'c' || expansion)) {
            auto previous = parsed.find(name);
            if (previous != parsed.end() && previous->second != digest) throw Error("Conflicting signaturefile records");
            parsed[name] = digest;
        }
    }
    if (parsed.empty()) throw Error("No applicable signaturefile records");
    records = std::move(parsed);
}
}}}
