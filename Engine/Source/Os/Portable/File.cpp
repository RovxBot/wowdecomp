#include "Engine/Source/Os/OsFile.h"
#include "Engine/Source/BlizzardCore/Portable/FilePlatform.h"
#include <algorithm>
#include <cerrno>
#include <cstring>

namespace F = Blizzard::File;
namespace {
F::StreamRecord* Stream(HOSFILE file) {
    if (!file || file == OS_INVALID_FILE) { NativeFile::Fail(EBADF); return nullptr; }
    return reinterpret_cast<F::StreamRecord*>(file);
}
bool Transfer(HOSFILE handle, void* buffer, uint32_t request, uint32_t* count, bool write) {
    if (!count) return NativeFile::Fail(EINVAL);
    *count = 0;
    auto file = Stream(handle);
    if (!file || (request && !buffer)) return NativeFile::Fail(EINVAL);
    while (*count < request) {
        int32_t amount = static_cast<int32_t>(std::min<uint32_t>(request - *count, INT32_MAX));
        const int32_t wanted = amount;
        auto data = static_cast<unsigned char*>(buffer) + *count;
        if (!(write ? F::Write(file, data, &amount) : F::Read(file, data, &amount))) return false;
        *count += amount;
        if (amount != wanted) break;
    }
    return true;
}
}
bool OsCloseFile(HOSFILE file) { auto stream = Stream(file); return stream && F::Close(stream); }
int32_t OsFileExists(const char* name) { return F::IsFile(name); }
int32_t OsDirectoryExists(const char* name) { return F::IsDirectory(name); }
int32_t OsReadFile(HOSFILE file, void* data, uint32_t size, uint32_t* count) { return Transfer(file, data, size, count, false); }
int32_t OsWriteFile(HOSFILE file, const void* data, uint32_t size, uint32_t* count) { return Transfer(file, const_cast<void*>(data), size, count, true); }
uint64_t OsGetFileSize(HOSFILE file) {
    auto stream = Stream(file);
    F::FileInfo info{};
    return stream && F::GetFileInfo(stream, &info) ? static_cast<uint64_t>(info.m_size) : UINT64_MAX;
}
uint32_t OsGetFileAttributes(const char* name) {
    F::FileInfo info{};
    if (!F::GetFileInfo(name, &info)) return UINT32_MAX;
    return ((info.m_attributes & 0x40) ? OS_FILE_ATTRIBUTE_DIRECTORY : OS_FILE_ATTRIBUTE_NORMAL) |
           (info.m_attributes & 1);
}
int32_t OsSetFileAttributes(const char* name, uint32_t attributes) {
    if (attributes & ~uint32_t{0x91}) return NativeFile::Fail(ENOTSUP);
    return F::SetAttributes(name, (attributes & 1) | ((attributes & OS_FILE_ATTRIBUTE_DIRECTORY) ? 0x40 : 0x20));
}
int32_t OsMoveFile(const char* from, const char* to) { return F::Move(from, to); }
int32_t OsCopyFile(const char* from, const char* to, int32_t failIfExists) { return F::Copy(from, to, !failIfExists); }
int32_t OsDeleteFile(const char* name) { return F::Delete(name); }
int32_t OsCreateDirectory(const char* name, int32_t recursive) { return F::CreateDirectory(name, recursive != 0); }
int32_t OsRemoveDirectory(const char* name) { return F::RemoveDirectory(name); }
int32_t OsRemoveDirectoryRecurse(const char* name, uint32_t flags) {
    if (flags & ~uint32_t{1}) return NativeFile::Fail(EINVAL);
    return F::RemoveDirectoryAndContents(name, flags != 0);
}
bool OsSetCurrentDirectory(const char* name) { return F::SetWorkingDirectory(name); }
int32_t OsGetCurrentDirectory(uint32_t size, char* buffer) {
    if (size > INT32_MAX) return NativeFile::Fail(EOVERFLOW);
    return F::GetWorkingDirectory(buffer, static_cast<int32_t>(size));
}
HOSFILE OsCreateFile(const char* name, uint32_t access, uint32_t share, uint32_t disposition, uint32_t attributes, uint32_t extended) {
    using D = F::Native::Disposition;
    if ((access & ~uint32_t{0xC0000000}) || !access || (share & ~uint32_t{3}) ||
        disposition < 1 || disposition > 5 || (extended && extended != OS_FILE_TYPE_ANY)) {
        NativeFile::Fail(EINVAL);
        return OS_INVALID_FILE;
    }
    if (attributes & ~uint32_t{0x81}) { NativeFile::Fail(ENOTSUP); return OS_INVALID_FILE; }
    const D dispositions[] = {D::CreateNew, D::CreateAlways, D::OpenExisting, D::OpenAlways, D::TruncateExisting};
    const int32_t mode = ((access & OS_GENERIC_READ) ? 1 : 0) | ((access & OS_GENERIC_WRITE) ? 2 : 0) | (share << 2);
    F::StreamRecord* file = nullptr;
    if (!F::Native::Open(name, mode, dispositions[disposition - 1], file)) return OS_INVALID_FILE;
    if ((attributes & 1) && !F::SetAttributes(name, 1 | 0x20)) { F::Close(file); return OS_INVALID_FILE; }
    return reinterpret_cast<HOSFILE>(file);
}
bool OsFileList_Internal::Callback(const F::ProcessDirParms& parms) {
    auto& list = *static_cast<OsFileList_Internal*>(parms.m_cookie);
    if (!F::SimpleGlob(parms.m_filename, list.m_pattern)) return true;
    if (!list.m_includeHidden && parms.m_filename[0] == '.') return true;
    const auto path = std::filesystem::u8path(parms.m_pathRoot) / std::filesystem::u8path(parms.m_filename);
    F::FileInfo info{};
    if (!F::GetFileInfo(path.u8string().c_str(), &info)) return false;
    OS_FILE_DATA data{};
    const size_t length = std::strlen(parms.m_filename);
    if (length >= sizeof(data.fileName)) return NativeFile::Fail(ENAMETOOLONG);
    std::memcpy(data.fileName, parms.m_filename, length + 1);
    data.size = static_cast<uint64_t>(info.m_size);
    data.flags = (parms.m_isDir ? OS_FILE_ATTRIBUTE_DIRECTORY : 0) | (info.m_attributes & 1);
    return !list.m_callback(data, list.m_param);
}
int32_t OsFileList(const char* directory, const char* pattern, OsFileListCallback callback, void* parameter, int32_t hidden) {
    if (!pattern || !callback) return NativeFile::Fail(EINVAL);
    OsFileList_Internal list{callback, parameter, hidden, pattern};
    return F::ProcessDirFast(directory, &list, OsFileList_Internal::Callback, true);
}
char* OsFileGetDownloadFolder() {
    // No native downloader is integrated. Do not silently write to the CWD/data.
    NativeFile::Fail(ENOTSUP);
    return nullptr;
}
