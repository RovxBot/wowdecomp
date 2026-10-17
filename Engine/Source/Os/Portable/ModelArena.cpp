#include "ModelArena.h"
#include <cstring>
#include <limits>
#include <stdexcept>
#if defined(WOWDECOMP_OS_WINDOWS)
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace OsNative {
namespace {
[[noreturn]] void Fail() { throw std::runtime_error("Cannot satisfy owner-scoped M2 relative-array allocation"); }
size_t Extent(uint32_t count, size_t size) {
    if (!size || count > std::numeric_limits<size_t>::max() / size) Fail();
    return count * size;
}
}
ModelArena::ModelArena(size_t bytes) {
#if defined(WOWDECOMP_OS_WINDOWS)
    SYSTEM_INFO info{}; GetSystemInfo(&info); page = info.dwPageSize;
#else
    long result = sysconf(_SC_PAGESIZE);
    if (result <= 0) Fail();
    page = static_cast<size_t>(result);
#endif
    if (!bytes || bytes > size_t(1024) * 1024 * 1024 || (page & (page - 1))) Fail();
    capacity = (bytes + page - 1) & ~(page - 1);
#if defined(WOWDECOMP_OS_WINDOWS)
    base = VirtualAlloc(nullptr, capacity, MEM_RESERVE, PAGE_NOACCESS);
#else
    base = mmap(nullptr, capacity, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (base == MAP_FAILED) base = nullptr;
#endif
    if (!base) Fail();
}
ModelArena::~ModelArena() {
    if (!base) return;
#if defined(WOWDECOMP_OS_WINDOWS)
    VirtualFree(base, 0, MEM_RELEASE);
#else
    munmap(base, capacity);
#endif
}
void* ModelArena::Allocate(size_t bytes, size_t alignment) {
    if (!bytes || !alignment || alignment > page || (alignment & (alignment - 1))) Fail();
    if (used > capacity - (alignment - 1)) Fail();
    size_t offset = (used + alignment - 1) & ~(alignment - 1);
    if (offset > capacity || bytes > capacity - offset) Fail();
    size_t end = offset + bytes;
    size_t newCommitted = (end + page - 1) & ~(page - 1);
    if (newCommitted > committed) {
        auto address = static_cast<uint8_t*>(base) + committed;
#if defined(WOWDECOMP_OS_WINDOWS)
        if (VirtualAlloc(address, newCommitted - committed, MEM_COMMIT, PAGE_READWRITE) != address) Fail();
#else
        if (mprotect(address, newCommitted - committed, PROT_READ | PROT_WRITE)) Fail();
#endif
        committed = newCommitted;
    }
    used = end;
    return static_cast<uint8_t*>(base) + offset;
}
bool ModelArena::Contains(const void* pointer, size_t bytes) const {
    uintptr_t start = reinterpret_cast<uintptr_t>(base), target = reinterpret_cast<uintptr_t>(pointer);
    return target >= start && target - start <= used && bytes <= used - (target - start);
}
void ModelArena::Encode(RelativeArray* anchor, const void* target, uint32_t count, size_t elementSize) const {
    if (!Contains(anchor, sizeof(*anchor)) || reinterpret_cast<uintptr_t>(anchor) % alignof(RelativeArray)) Fail();
    if (!count) { *anchor = {0, 0}; return; }
    if (!Contains(target, Extent(count, elementSize))) Fail();
    auto a = reinterpret_cast<uintptr_t>(anchor), b = reinterpret_cast<uintptr_t>(target);
    int64_t difference = b >= a ? static_cast<int64_t>(b - a) : -static_cast<int64_t>(a - b);
    if (difference < INT32_MIN || difference > INT32_MAX) Fail();
    *anchor = {count, static_cast<int32_t>(difference)};
}
const void* ModelArena::Resolve(const RelativeArray* anchor, size_t elementSize) const {
    if (!Contains(anchor, sizeof(*anchor)) || reinterpret_cast<uintptr_t>(anchor) % alignof(RelativeArray)) Fail();
    if (!anchor->count) {
        if (anchor->offset) Fail();
        return nullptr;
    }
    uintptr_t a = reinterpret_cast<uintptr_t>(anchor);
    // The reservation is <=1 GiB. Reject invalid offsets before forming pointers.
    uintptr_t start = reinterpret_cast<uintptr_t>(base);
    int64_t offset = static_cast<int64_t>(a - start) + anchor->offset;
    if (offset < 0 || uint64_t(offset) > used) Fail();
    auto target = static_cast<const uint8_t*>(base) + offset;
    if (!Contains(target, Extent(anchor->count, elementSize))) Fail();
    return target;
}
}
