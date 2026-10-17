#include <algorithm>
#include <atomic>
#include <cerrno>
#include <unistd.h>

namespace {
std::atomic<int> readFault{0}, writeFault{0};
}
void SetReadFault(int fault) { readFault = fault; }
void SetWriteFault(int fault) { writeFault = fault; }
extern "C" ssize_t __real_pread(int, void*, size_t, off_t);
extern "C" ssize_t __real_pwrite(int, const void*, size_t, off_t);
extern "C" ssize_t __wrap_pread(int fd, void* data, size_t count, off_t offset) {
    switch (readFault.exchange(0)) {
        case 1: errno = EINTR; return -1;
        case 2: count = std::min<size_t>(count, 2); break;
        case 3: errno = EIO; return -1;
    }
    return __real_pread(fd, data, count, offset);
}
extern "C" ssize_t __wrap_pwrite(int fd, const void* data, size_t count, off_t offset) {
    switch (writeFault.exchange(0)) {
        case 1: errno = EINTR; return -1;
        case 2: count = std::min<size_t>(count, 2); break;
        case 3: errno = EIO; return -1;
    }
    return __real_pwrite(fd, data, count, offset);
}
