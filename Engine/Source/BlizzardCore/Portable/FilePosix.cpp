#include "Engine/Source/BlizzardCore/Portable/FilePlatform.h"
#include <cerrno>
#include <climits>
#include <cstdio>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#if defined(WOWDECOMP_OS_LINUX)
#include <sys/syscall.h>
#include <linux/fs.h>
#elif defined(WOWDECOMP_OS_MACOS)
#include <stdio.h>
#endif

namespace NativeFile {
namespace {
bool Timestamp(int64_t seconds, long nanos, int64_t& result) {
    const __int128 value = (static_cast<__int128>(seconds) - 946684800) * 1000000000 + nanos;
    if (value < INT64_MIN || value > INT64_MAX) return Fail(EOVERFLOW);
    result = static_cast<int64_t>(value);
    return true;
}
bool Fill(const struct stat& status, Blizzard::File::FileInfo& info) {
    info = {};
    info.m_size = status.st_size;
    info.m_regularFile = S_ISREG(status.st_mode);
    info.m_existence = S_ISDIR(status.st_mode) ? 2 : 1;
    info.m_attributes = S_ISDIR(status.st_mode) ? 0x40 : 0x20;
    if (!(status.st_mode & 0222)) info.m_attributes |= 1;
#if defined(WOWDECOMP_OS_MACOS)
    if (!Timestamp(status.st_birthtimespec.tv_sec, status.st_birthtimespec.tv_nsec, info.m_createTime)) return false;
    return Timestamp(status.st_mtimespec.tv_sec, status.st_mtimespec.tv_nsec, info.m_modTime) &&
           Timestamp(status.st_atimespec.tv_sec, status.st_atimespec.tv_nsec, info.m_accessTime);
#else
    // stat has no birth time on Linux. Zero means unavailable here, not ctime.
    return Timestamp(status.st_mtim.tv_sec, status.st_mtim.tv_nsec, info.m_modTime) &&
           Timestamp(status.st_atim.tv_sec, status.st_atim.tv_nsec, info.m_accessTime);
#endif
}
}

bool Open(const std::filesystem::path& path, int32_t access, Blizzard::File::Native::Disposition disposition, Handle& result) {
    using D = Blizzard::File::Native::Disposition;
    result = Invalid;
    // Nonblocking open lets the regular-file check reject FIFOs without waiting
    // for a peer. POSIX ignores this flag for subsequent regular-file I/O.
    int flags = O_CLOEXEC | O_NONBLOCK | ((access & 3) == 3 ? O_RDWR : (access & 2) ? O_WRONLY : O_RDONLY);
    switch (disposition) {
        case D::CreateNew: flags |= O_CREAT | O_EXCL; break;
        case D::CreateAlways: flags |= O_CREAT | O_TRUNC; break;
        case D::OpenAlways: flags |= O_CREAT; break;
        case D::TruncateExisting: flags |= O_TRUNC; break;
        case D::OpenExisting: break;
    }
    int fd;
    do { fd = ::open(path.c_str(), flags, 0666); } while (fd < 0 && errno == EINTR);
    if (fd < 0) return Fail(errno);
    struct stat status{};
    if (fstat(fd, &status) != 0 || !S_ISREG(status.st_mode)) {
        const int error = errno;
        ::close(fd);
        return Fail(S_ISREG(status.st_mode) ? error : EINVAL);
    }
    result = fd;
    return true;
}
bool Close(Handle file) {
    // A retry can close a different thread's reused descriptor on Linux.
    return ::close(static_cast<int>(file)) == 0 || Fail(errno);
}
bool Transfer(Handle file, void* data, int64_t offset, int32_t& count, bool write) {
    static_assert(sizeof(off_t) >= sizeof(int64_t), "64-bit file offsets are required");
    ssize_t amount;
    do {
        amount = write ? pwrite(static_cast<int>(file), data, static_cast<size_t>(count), offset) :
                         pread(static_cast<int>(file), data, static_cast<size_t>(count), offset);
    } while (amount < 0 && errno == EINTR);
    if (amount < 0) { count = 0; return Fail(errno); }
    count = static_cast<int32_t>(amount);
    return true;
}
bool Flush(Handle file) {
    int result;
    do { result = fsync(static_cast<int>(file)); } while (result < 0 && errno == EINTR);
    return result == 0 || Fail(errno);
}
bool Resize(Handle file, int64_t size) {
    int result;
    do { result = ftruncate(static_cast<int>(file), size); } while (result < 0 && errno == EINTR);
    return result == 0 || Fail(errno);
}
bool Info(Handle file, Blizzard::File::FileInfo& info) {
    struct stat status{};
    if (fstat(static_cast<int>(file), &status) != 0) return Fail(errno);
    return Fill(status, info);
}
bool Info(const std::filesystem::path& path, Blizzard::File::FileInfo& info) {
    struct stat status{};
    if (stat(path.c_str(), &status) != 0) return Fail(errno);
    return Fill(status, info);
}
bool Move(const std::filesystem::path& from, const std::filesystem::path& to) {
#if defined(WOWDECOMP_OS_LINUX)
    return syscall(SYS_renameat2, AT_FDCWD, from.c_str(), AT_FDCWD, to.c_str(), RENAME_NOREPLACE) == 0 || Fail(errno);
#elif defined(WOWDECOMP_OS_MACOS)
    return renameatx_np(AT_FDCWD, from.c_str(), AT_FDCWD, to.c_str(), RENAME_EXCL) == 0 || Fail(errno);
#endif
}
bool Attributes(const std::filesystem::path& path, int32_t attributes) {
    if (attributes & ~0x61) return Fail(ENOTSUP);
    struct stat status{};
    if (stat(path.c_str(), &status) != 0) return Fail(errno);
    mode_t mode = attributes & 1 ? status.st_mode & ~mode_t{0222} : status.st_mode | S_IWUSR;
    return chmod(path.c_str(), mode & 07777) == 0 || Fail(errno);
}
bool Replace(const std::filesystem::path& from, const std::filesystem::path& to) {
    return rename(from.c_str(), to.c_str()) == 0 || Fail(errno);
}
}
