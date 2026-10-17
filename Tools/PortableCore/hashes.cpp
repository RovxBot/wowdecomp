// Asset-free byte/streaming fixtures for the existing 12340 hash implementations.
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include "Engine/Source/Storm/Source/MD5.h"
#include "Engine/Source/Storm/Source/Sha1.h"

#if defined(WOWDECOMP_OS_LINUX) || defined(WOWDECOMP_OS_MACOS)
#if defined(WIN32) || defined(_WIN32) || defined(_WINDOWS) || defined(_USE_32BIT_TIME_T)
#error Windows definitions leaked into the native core
#endif
#endif
#if defined(WOWDECOMP_CPU_ARM64) && (defined(__SSE__) || defined(_M_X64))
#error ARM64 must not select x86 implementations
#endif
static_assert(sizeof(void*) * 8 ==
#if defined(WOWDECOMP_ARCH_64)
    64
#else
    32
#endif
);

struct Vector { uint32_t size; const char* md5; const char* sha1; };
// Generated with Python hashlib from byte[i] = (i * 37 + 128) % 256.
static const Vector vectors[] = {
    {0, "d41d8cd98f00b204e9800998ecf8427e", "da39a3ee5e6b4b0d3255bfef95601890afd80709"},
    {1, "8d39dd7eef115ea6975446ef4082951f", "c78ebd3c85a39a596d9f5cfd2b8d240bc1b9c125"},
    {3, "e9436257a8ca56b318ea74676f34ad35", "2254799b7a26ec7fcec44f9d0c07ae42a7833bb0"},
    {55, "05b6f147013ad96f3964c9eeb56d8baf", "45d049142cd1784ad869a7ee26bb653d8d78cd0c"},
    {56, "eb89fb7dc809845b334abe86a2a47a20", "94b97886a71dec77f5f758e1b2e2d8ec0e1d40cf"},
    {63, "f4768d343cac289c9b58f06c36906fa4", "a7890ea2247fc3cf16ccec790831de7e3c40017d"},
    {64, "65aa793f755efddcbfdd26d69bffec92", "14a6622a054ade80bda56f08970fd15330f671a9"},
    {65, "1e1d9a93592c75f925af743eb472b6fd", "0a61aa4958cbb560963ec81d28a69c5cf0f67ff4"},
    {127, "64f86c54f82a0c18c344a0f80746f7b8", "0a879cb05e1639fcb9b09a40180d8212e76482fa"},
    {128, "4469de88981f837969c9d36b25621796", "1b5fdafef3886e2a568d6a25550278f62e8b9d93"},
    {129, "d9defda2f2b6790db1501fae9a5b5083", "be1670699fca66133c973aa3f8cfd57166e340ad"},
    {1000, "6b27da5d62d1adce3a6c37a1c6f5444a", "f1a98874aea2d8564f5ec5f32b8dcf18ce200dd2"},
};

static std::string Hex(const uint8_t* bytes, size_t count) {
    static const char digits[] = "0123456789abcdef";
    std::string result;
    for (size_t i = 0; i < count; ++i) {
        result += digits[bytes[i] >> 4];
        result += digits[bytes[i] & 15];
    }
    return result;
}

static bool Check(const uint8_t* data, uint32_t size, uint32_t chunk,
                  const char* expectedMd5, const char* expectedSha1) {
    MD5_CTX md5;
    Sha1 sha1;
    MD5Init(&md5);
    sha1.Initialize();
    // Exercise empty updates using a valid pointer as well as chunk boundaries.
    MD5Update(&md5, data, 0);
    sha1.Append(data, 0);
    for (uint32_t offset = 0; offset < size;) {
        uint32_t count = std::min(chunk, size - offset);
        MD5Update(&md5, data + offset, count);
        sha1.Append(data + offset, count);
        offset += count;
    }
    uint8_t md5Result[16];
    uint8_t sha1Result[20];
    MD5Final(md5Result, &md5);
    sha1.Finalize(sha1Result);
    if (Hex(md5Result, sizeof(md5Result)) != expectedMd5 ||
        Hex(sha1Result, sizeof(sha1Result)) != expectedSha1) {
        std::fprintf(stderr, "Hash mismatch: size=%u chunk=%u MD5=%s SHA1=%s\n",
                     size, chunk, Hex(md5Result, 16).c_str(), Hex(sha1Result, 20).c_str());
        return false;
    }
    return true;
}

int main() {
    // An offset pointer also exercises inputs without word alignment.
    uint8_t storage[1001];
    for (uint32_t i = 0; i < 1000; ++i) {
        storage[i + 1] = static_cast<uint8_t>(i * 37 + 128);
    }
    bool passed = true;
    for (const auto& vector : vectors) {
        for (uint32_t chunk : {1u, 7u, 64u, 65u, 131u, 1000u}) {
            passed &= Check(storage + 1, vector.size, chunk, vector.md5, vector.sha1);
        }
    }
    passed &= Check(reinterpret_cast<const uint8_t*>("abc"), 3, 3,
                    "900150983cd24fb0d6963f7d28e17f72", "a9993e364706816aba3e25717850c26c9cd0d89d");
    if (!passed) return 1;
    std::puts("Portable core: MD5/SHA-1 known-answer and streaming checks passed.");
    return 0;
}
