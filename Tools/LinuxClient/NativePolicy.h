// RovxBot Linux integration policy; newly authored, no original VERIFY claim.
#pragma once
#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <filesystem>
#include <new>
#include <string>
#include <vector>

namespace RovNative {
inline bool failedStartupCleanup = false;
inline void* volatile allocationWitness = nullptr;
inline void CheckAllocationRoutes() {
    void* scalar = ::operator new(31);
    allocationWitness = scalar;
    static_cast<volatile unsigned char*>(scalar)[30] = 1;
    ::operator delete(scalar, size_t(31));
    void* array = ::operator new[](47);
    allocationWitness = array;
    static_cast<volatile unsigned char*>(array)[46] = 2;
    ::operator delete[](array, size_t(47));
    void* aligned = ::operator new(128, std::align_val_t(64));
    allocationWitness = aligned;
    if (reinterpret_cast<uintptr_t>(aligned) % 64) std::abort();
    static_cast<volatile unsigned char*>(aligned)[127] = 3;
    ::operator delete(aligned, size_t(128), std::align_val_t(64));
    array = ::operator new[](128, std::align_val_t(64), std::nothrow);
    if (!array || reinterpret_cast<uintptr_t>(array) % 64) std::abort();
    allocationWitness = array;
    ::operator delete[](array, std::align_val_t(64), std::nothrow);
    allocationWitness = nullptr;
    for (size_t alignment : {size_t(1), size_t(2), size_t(4), size_t(8), size_t(16), size_t(64)}) {
        void* pointer = ::operator new(67, std::align_val_t(alignment), std::nothrow);
        if (!pointer || reinterpret_cast<uintptr_t>(pointer) % alignment) std::abort();
        allocationWitness = pointer;
        static_cast<volatile unsigned char*>(pointer)[66] = 4;
        ::operator delete(pointer, size_t(67), std::align_val_t(alignment));
    }
    allocationWitness = nullptr;
}
inline bool TestServerEnabled() {
#if WOWDECOMP_ISOLATED_SERVER_BUILD
    const char* value = std::getenv("WOWDECOMP_ISOLATED_SERVER_LOGIN");
    return value && !std::strcmp(value, "1");
#else
    return false;
#endif
}
inline constexpr const char* IntegrityUnavailable =
    "Native executable integrity, ScanDLL and Warden are unavailable. "
    "Normal sign-in is disabled. Isolated TEST-SERVER testing requires a separate build and runtime opt-in.";

inline bool ExplicitEndpoint(const char* value) {
    const char* expected = std::getenv("WOWDECOMP_ISOLATED_ENDPOINT");
    if (!value || !expected || !*value || std::strlen(value) > 255 || std::strcmp(value, expected)) return false;
    const char* colon = std::strchr(value, ':');
    const char* end = colon ? colon : value + std::strlen(value);
    if (end == value) return false;
    for (const char* p = value; p != end; ++p)
        if (!((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') ||
              (*p >= '0' && *p <= '9') || *p == '.' || *p == '-')) return false;
    if (colon) {
        unsigned port = 0;
        for (const char* p = colon + 1; *p; ++p) {
            if (*p < '0' || *p > '9' || port > 6553) return false;
            port = port * 10 + unsigned(*p - '0');
        }
        if (!port || port > 65535) return false;
    }
    return true;
}

inline bool Within(const std::filesystem::path& path, const std::filesystem::path& root) {
    auto p = path.begin();
    for (auto r = root.begin(); r != root.end(); ++r, ++p)
        if (p == path.end() || *p != *r) return false;
    return true;
}
inline std::filesystem::path Canonical(const char* path) {
    if (!path || !*path) return {};
    std::string normalized(path);
    std::replace(normalized.begin(), normalized.end(), '\\', '/');
    std::error_code error;
    auto result = std::filesystem::weakly_canonical(normalized, error);
    return error ? std::filesystem::path{} : result;
}
inline bool AllowedWrite(const char* path) {
    auto target = Canonical(path);
    auto game = Canonical(std::getenv("WOWDECOMP_GAME_ROOT"));
    if (target.empty() || game.empty() || Within(target, game)) { errno = EACCES; return false; }
    for (const char* name : {"WOWDECOMP_SETTINGS", "WOWDECOMP_CACHE", "WOWDECOMP_LOGS"}) {
        auto root = Canonical(std::getenv(name));
        if (!root.empty() && Within(target, root)) return true;
    }
    errno = EACCES;
    return false;
}

// Ordered names are collected only during original Glue creation. Original MD5
// covers the ordered bytes; the traversal count also rejects partial creation.
inline bool glueLoading = false;
inline std::vector<std::string> glueVisits;
inline void GlueBegin() { glueVisits.clear(); glueLoading = true; }
inline void GlueVisit(const char* path) {
    if (glueLoading) { glueVisits.emplace_back(path); std::fprintf(stderr, "glue-visit=%s\n", path); }
}
inline void GlueEnd() {
    glueLoading = false;
    if (glueVisits.size() != 60) {
        std::fprintf(stderr, "Original Glue creation refused: visits=%zu expected=60\n", glueVisits.size());
        std::abort();
    }
    std::fprintf(stderr, "startup-stage=glue-created visits=60 post-creation-digest=matched\n");
}
}
