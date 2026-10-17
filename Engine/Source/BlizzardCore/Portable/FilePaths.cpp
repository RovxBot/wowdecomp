#include "Engine/Source/BlizzardCore/Portable/FilePlatform.h"
#include <algorithm>
#include <cerrno>

namespace fs = std::filesystem;
namespace {
// Existing equivalent ancestors also catch Windows spelling aliases. This is
// path routing for a stable installation, not protection against hostile rename
// or symlink races by another process.
bool Inside(fs::path child, const fs::path& parent) {
    while (!child.empty()) {
        std::error_code error;
        if (child == parent || fs::equivalent(child, parent, error)) return true;
        const auto next = child.parent_path();
        if (next == child) break;
        child = next;
    }
    return false;
}
bool Absolute(const char* name, fs::path& result) {
    if (!NativeFile::HostPath(name, result)) return false;
    std::error_code error;
    result = fs::absolute(result, error);
    if (!error) result = fs::weakly_canonical(result, error);
    return !error || NativeFile::Fail(error);
}
bool Relative(const char* name, fs::path& result) {
    if (!NativeFile::HostPath(name, result)) return false;
    if (result.has_root_path() || result.empty()) return NativeFile::Fail(EINVAL);
    for (const auto& piece : result) {
        // ':' additionally excludes drive-relative Windows names on POSIX.
        if (piece == ".." || piece == "." || piece.empty() || piece.u8string().find(':') != std::string::npos)
            return NativeFile::Fail(EINVAL);
    }
    return true;
}
std::string Fold(std::string text) {
    for (char& c : text) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    return text;
}
bool Contained(const fs::path& root, const fs::path& candidate, std::string& result) {
    std::error_code error;
    const auto real = fs::weakly_canonical(candidate, error);
    if (error) return NativeFile::Fail(error);
    if (!Inside(real, root)) return NativeFile::Fail(EACCES);
    result = real.u8string();
    return true;
}
}
namespace Blizzard { namespace File { namespace Native {
bool ConfigurePaths(const char* dataName, const char* settingsName, const char* cacheName, Paths& result) {
    fs::path data, settings, cache;
    if (!Absolute(dataName, data) || !Absolute(settingsName, settings) || !Absolute(cacheName, cache)) return false;
    if (Inside(settings, data) || Inside(data, settings) || Inside(cache, data) || Inside(data, cache) ||
        Inside(cache, settings) || Inside(settings, cache)) return NativeFile::Fail(EINVAL);
    std::error_code error;
    if (!fs::is_directory(data, error)) return NativeFile::Fail(error ? error : std::make_error_code(std::errc::not_a_directory));
    if (!CreateDirectory(settings.u8string().c_str(), true) || !CreateDirectory(cache.u8string().c_str(), true)) return false;
    result = {data.u8string(), settings.u8string(), cache.u8string()};
    return true;
}
bool ResolveDataPath(const Paths& paths, const char* name, std::string& result) {
    result.clear();
    fs::path relative, root;
    if (!Relative(name, relative) || !Absolute(paths.data.c_str(), root)) return false;
    fs::path current = root;
    for (const auto& piece : relative) {
        std::error_code error;
        fs::directory_iterator entry(current, error), end;
        if (error) return NativeFile::Fail(error);
        fs::path match;
        const std::string key = Fold(piece.u8string());
        for (; entry != end; entry.increment(error)) {
            if (error) return NativeFile::Fail(error);
            if (Fold(entry->path().filename().u8string()) == key) {
                if (!match.empty()) return NativeFile::Fail(EEXIST);
                match = entry->path();
            }
        }
        if (error) return NativeFile::Fail(error);
        if (match.empty()) return NativeFile::Fail(ENOENT);
        std::string contained;
        if (!Contained(root, match, contained)) return false;
        current = fs::u8path(contained);
    }
    result = current.u8string();
    return true;
}
bool OpenData(const Paths& paths, const char* relative, StreamRecord*& file) {
    file = nullptr;
    std::string path;
    return ResolveDataPath(paths, relative, path) && Open(path.c_str(), 1 | 4, Disposition::OpenExisting, file);
}
bool ResolveUserPath(const Paths& paths, UserRoot which, const char* name, std::string& result) {
    result.clear();
    if (which != UserRoot::Settings && which != UserRoot::Cache) return NativeFile::Fail(EINVAL);
    fs::path relative, root;
    if (!Relative(name, relative) || !Absolute((which == UserRoot::Settings ? paths.settings : paths.cache).c_str(), root)) return false;
    return Contained(root, root / relative, result);
}
}}}
