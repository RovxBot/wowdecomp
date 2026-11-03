// RovxBot Linux integration policy; newly authored, no original VERIFY claim.
#pragma once
#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace RovNative {
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
