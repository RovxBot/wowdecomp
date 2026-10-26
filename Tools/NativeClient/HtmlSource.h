// Original SimpleHTML's locale data files, using the existing bounded resolver
// and retained worker. Interface Lua/XML/TOC readers never use this route.
#ifndef WOWDECOMP_NATIVE_CLIENT_HTML_SOURCE_H
#define WOWDECOMP_NATIVE_CLIENT_HTML_SOURCE_H
#include "Engine/Source/BlizzardCore/Portable/File.h"
#include "Engine/Source/Services/Portable/AsyncFile.h"
#include <algorithm>
#include <stdexcept>
namespace GlueNative {
inline bool ReadLocaleHtml(const Blizzard::File::Native::Paths& paths,
    Services::Native::FileQueue& queue, const std::string& name, std::vector<uint8_t>& bytes) {
    constexpr uint32_t Limit = 8 * 1024 * 1024;
    bytes.clear();
    // This startup currently selects enUS. Admit only original HTML basenames,
    // never arbitrary host paths or a fallback for missing interface sources.
    if (name.size() < 6 || name.size() >= 260 || name.find_first_of("/\\:") != std::string::npos) return false;
    auto suffix = name.substr(name.size() - 5);
    for (auto& c : suffix) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    if (suffix != ".html") return false;
    std::string path;
    if (!Blizzard::File::Native::ResolveDataPath(paths, ("enUS/" + name).c_str(), path)) return false;
    Blizzard::File::FileInfo info{};
    if (!Blizzard::File::GetFileInfo(path.c_str(), &info) || !info.m_regularFile || info.m_size < 0 || info.m_size > Limit) return false;
    auto result = std::make_shared<Services::Native::ReadResult>();
    auto ticket = queue.ReadHost(path, 0, Limit + 1,
        [result](const Services::Native::ReadResult& value) { *result = value; });
    if (!queue.Wait(ticket, std::chrono::seconds(60))) throw std::runtime_error("Original locale HTML read timed out: " + name);
    if (result->status != Services::Native::ReadStatus::Success || result->file.bytes.size() != uint64_t(info.m_size)) return false;
    bytes = std::move(result->file.bytes); return true;
}
}
#endif
