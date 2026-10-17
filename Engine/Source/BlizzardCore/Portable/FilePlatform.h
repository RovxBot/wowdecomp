#ifndef WOWDECOMP_FILE_PLATFORM_H
#define WOWDECOMP_FILE_PLATFORM_H
#include "Engine/Source/BlizzardCore/Portable/File.h"
#include <filesystem>
#include <system_error>

namespace NativeFile {
using Handle = intptr_t;
constexpr Handle Invalid = -1;
bool Fail(int error);
bool Fail(const std::error_code& error);
bool HostPath(const char* input, std::filesystem::path& result);
bool Open(const std::filesystem::path& path, int32_t access, Blizzard::File::Native::Disposition disposition, Handle& result);
bool Close(Handle file);
bool Transfer(Handle file, void* data, int64_t offset, int32_t& count, bool write);
bool Flush(Handle file);
bool Resize(Handle file, int64_t size);
bool Info(Handle file, Blizzard::File::FileInfo& result);
bool Info(const std::filesystem::path& path, Blizzard::File::FileInfo& result);
bool Move(const std::filesystem::path& from, const std::filesystem::path& to);
bool Replace(const std::filesystem::path& from, const std::filesystem::path& to);
bool Attributes(const std::filesystem::path& path, int32_t attributes);
}
#endif
