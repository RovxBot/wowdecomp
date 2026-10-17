// Windows comparison provider for the native ABI. Never part of the legacy client.
#include "Engine/Source/BlizzardCore/Portable/FilePlatform.h"
#include "Engine/Source/Storm/H/storm.h"
#include <windows.h>
#include <cerrno>
#include <limits>
#undef MoveFile

namespace NativeFile {
namespace {
HANDLE HandleOf(Handle handle) { return reinterpret_cast<HANDLE>(handle); }
bool Error() { SErrSetLastError(::GetLastError()); return false; }
bool Timestamp(FILETIME time, int64_t& value) {
    const uint64_t ticks = (uint64_t{time.dwHighDateTime} << 32) | time.dwLowDateTime;
    constexpr uint64_t epoch = 125911584000000000ULL;
    const uint64_t delta = ticks >= epoch ? ticks - epoch : epoch - ticks;
    if (delta > static_cast<uint64_t>(INT64_MAX) / 100) return Fail(EOVERFLOW);
    value = static_cast<int64_t>(delta * 100) * (ticks >= epoch ? 1 : -1);
    return true;
}
bool Fill(const BY_HANDLE_FILE_INFORMATION& data, Blizzard::File::FileInfo& info) {
    info = {};
    const uint64_t size = (uint64_t{data.nFileSizeHigh} << 32) | data.nFileSizeLow;
    if (size > INT64_MAX) return Fail(EOVERFLOW);
    info.m_size = static_cast<int64_t>(size);
    info.m_existence = data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ? 2 : 1;
    info.m_regularFile = info.m_existence == 1;
    info.m_attributes = info.m_existence == 2 ? 0x40 : 0x20;
    if (data.dwFileAttributes & FILE_ATTRIBUTE_READONLY) info.m_attributes |= 1;
    return Timestamp(data.ftCreationTime, info.m_createTime) && Timestamp(data.ftLastWriteTime, info.m_modTime) &&
           Timestamp(data.ftLastAccessTime, info.m_accessTime);
}
}
bool Open(const std::filesystem::path& path, int32_t access, Blizzard::File::Native::Disposition disposition, Handle& result) {
    using D = Blizzard::File::Native::Disposition;
    result = Invalid;
    DWORD creation = OPEN_EXISTING;
    switch (disposition) {
        case D::CreateNew: creation = CREATE_NEW; break;
        case D::CreateAlways: creation = CREATE_ALWAYS; break;
        case D::OpenAlways: creation = OPEN_ALWAYS; break;
        case D::TruncateExisting: creation = TRUNCATE_EXISTING; break;
        case D::OpenExisting: break;
    }
    const DWORD desired = ((access & 1) ? GENERIC_READ : 0) | ((access & 2) ? GENERIC_WRITE : 0);
    const DWORD share = ((access & 4) ? FILE_SHARE_READ : 0) | ((access & 8) ? FILE_SHARE_WRITE : 0);
    HANDLE handle = CreateFileW(path.c_str(), desired, share, nullptr, creation, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) return Error();
    result = reinterpret_cast<Handle>(handle);
    return true;
}
bool Close(Handle file) { return CloseHandle(HandleOf(file)) || Error(); }
bool Transfer(Handle file, void* data, int64_t offset, int32_t& count, bool write) {
    LARGE_INTEGER position;
    position.QuadPart = offset;
    if (!SetFilePointerEx(HandleOf(file), position, nullptr, FILE_BEGIN)) { count = 0; return Error(); }
    DWORD amount = 0;
    const BOOL result = write ? WriteFile(HandleOf(file), data, static_cast<DWORD>(count), &amount, nullptr) :
                                ReadFile(HandleOf(file), data, static_cast<DWORD>(count), &amount, nullptr);
    count = static_cast<int32_t>(amount);
    return result || Error();
}
bool Flush(Handle file) { return FlushFileBuffers(HandleOf(file)) || Error(); }
bool Resize(Handle file, int64_t size) {
    FILE_END_OF_FILE_INFO info;
    info.EndOfFile.QuadPart = size;
    return SetFileInformationByHandle(HandleOf(file), FileEndOfFileInfo, &info, sizeof(info)) || Error();
}
bool Info(Handle file, Blizzard::File::FileInfo& info) {
    BY_HANDLE_FILE_INFORMATION data{};
    if (!GetFileInformationByHandle(HandleOf(file), &data)) return Error();
    return Fill(data, info);
}
bool Info(const std::filesystem::path& path, Blizzard::File::FileInfo& info) {
    HANDLE handle = CreateFileW(path.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (handle == INVALID_HANDLE_VALUE) return Error();
    const bool result = Info(reinterpret_cast<Handle>(handle), info);
    CloseHandle(handle);
    return result;
}
bool Move(const std::filesystem::path& from, const std::filesystem::path& to) {
    return MoveFileExW(from.c_str(), to.c_str(), 0) || Error();
}
bool Replace(const std::filesystem::path& from, const std::filesystem::path& to) {
    return MoveFileExW(from.c_str(), to.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) || Error();
}
bool Attributes(const std::filesystem::path& path, int32_t attributes) {
    if (attributes & ~0x61) return Fail(ENOTSUP);
    DWORD value = GetFileAttributesW(path.c_str());
    if (value == INVALID_FILE_ATTRIBUTES) return Error();
    value = attributes & 1 ? value | FILE_ATTRIBUTE_READONLY : value & ~FILE_ATTRIBUTE_READONLY;
    return SetFileAttributesW(path.c_str(), value) || Error();
}
}
