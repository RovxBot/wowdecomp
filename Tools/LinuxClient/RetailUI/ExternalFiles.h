#pragma once
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>

namespace RovRetailUI {
inline bool ExternalName(const char* name) {
    if (!name) return false;
    return ((!std::strncmp(name, "RovForeverUI", 12) && (name[12] == '/' || name[12] == '\\')) ||
            (!std::strncmp(name, "RovForeverFonts", 15) && (name[15] == '/' || name[15] == '\\')));
}
// Two explicit development namespaces. No loose-file override of the original
// signed Interface/Fonts paths, global direct-access toggle or CASC mount.
inline bool ExternalFile(const char* name, char* result, size_t capacity) {
    const char* version = std::getenv("WOWDECOMP_RETAIL_UI_COMPAT");
    const char* root = std::getenv("WOWDECOMP_FOREVER_ASSETS");
    if (!version || std::strcmp(version, "forever-1.60.1-70862-v1") || !root || !*root || !ExternalName(name)) return false;
    std::string relative(name);
    std::replace(relative.begin(), relative.end(), '\\', '/');
    const char* subdir;
    if (relative.rfind("RovForeverUI/", 0) == 0) { relative.erase(0, 13); subdir = "local-ui"; }
    else if (relative.rfind("RovForeverFonts/", 0) == 0) { relative.erase(0, 16); subdir = "assets"; }
    else return false;
    const std::filesystem::path tail(relative);
    if (tail.empty() || tail.is_absolute()) return false;
    std::error_code error;
    const auto base = std::filesystem::canonical(std::filesystem::path(root) / subdir, error);
    if (error) return false;
    const auto target = std::filesystem::canonical(base / tail, error);
    if (error || !std::filesystem::is_regular_file(target, error) || error) return false;
    auto p = target.begin();
    for (auto b = base.begin(); b != base.end(); ++b, ++p) if (p == target.end() || *p != *b) return false;
    const auto value = target.string();
    if (value.size() >= capacity) return false;
    std::memcpy(result, value.c_str(), value.size() + 1);
    return true;
}
}
