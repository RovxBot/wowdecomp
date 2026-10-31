// Digest semantics for the single DXVK dependency correction; no game data.
#include "sha1.h"
#include <array>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>

namespace {
void Digest(const std::string& input, size_t split, const char* expected) {
    SHA1_CTX context;
    SHA1Init(&context);
    SHA1Update(&context, nullptr, 0);
    SHA1Update(&context, reinterpret_cast<const uint8_t*>(input.data()), split);
    SHA1Update(&context, nullptr, 0);
    SHA1Update(&context, reinterpret_cast<const uint8_t*>(input.data()) + split, input.size() - split);
    SHA1Update(&context, nullptr, 0);
    std::array<uint8_t, SHA1_DIGEST_LENGTH> digest{};
    SHA1Final(digest.data(), &context);
    std::string actual;
    for (auto byte : digest) {
        char hex[3];
        std::snprintf(hex, sizeof(hex), "%02x", byte);
        actual += hex;
    }
    if (actual != expected) throw std::runtime_error("DXVK SHA1 digest changed");
}
}
int main() {
    try {
        Digest("", 0, "da39a3ee5e6b4b0d3255bfef95601890afd80709");
        Digest("abc", 1, "a9993e364706816aba3e25717850c26c9cd0d89d");
        Digest(std::string(63, 'a'), 31, "03f09f5b158a7a8cdad920bddc29b81c18a551f5");
        Digest(std::string(64, 'a'), 63, "0098ba824b5c16427bd7a1122a5a442a25ec644d");
        Digest(std::string(65, 'a'), 64, "11655326c708d70319be2610e8a57d9a5b959d3b");
        Digest(std::string(130, 'a'), 65, "e1cd437ec3e8a60db34e1d150a4fc73882d83b41");
        std::puts("dxvk-sha1-null-empty-and-split-block-vectors=passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
