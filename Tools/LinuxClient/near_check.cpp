#include "Engine/Source/Os/OsMemory.h"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <limits>
#include <sys/mman.h>

static void Require(bool condition, int line) {
    if (!condition) { std::fprintf(stderr, "near-memory contract failed at line %d\n", line); std::abort(); }
}
#define Check(condition) Require(bool(condition), __LINE__)
int main() {
    // ASan reserves large malloc regions, which can truthfully leave no nearby
    // address space. Use an explicitly mapped owner for the successful case;
    // the allocator must never fall back to an out-of-range malloc allocation.
    void* anchor = mmap(nullptr, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Check(anchor != MAP_FAILED);
    Check(!OsAllocNear(nullptr, 1024));
    Check(!OsAllocNear(anchor, std::numeric_limits<size_t>::max()));
    Check(!OsAllocNear(anchor, INT32_MAX));
    Check(!OsAllocNear(reinterpret_cast<void*>(UINTPTR_MAX - 1024), 4096));
    void* payload = OsAllocNear(anchor, 65536);
    Check(payload);
    auto base = reinterpret_cast<uintptr_t>(anchor);
    auto start = reinterpret_cast<uintptr_t>(payload);
    auto distance = start > base ? start - base : base - start;
    Check(distance < INT32_MAX - 65536);
    std::memset(payload, 0x5a, 65536);
    Check(static_cast<unsigned char*>(payload)[65535] == 0x5a);
    OsFreeNear(payload);
    Check(munmap(anchor, 4096) == 0);
}
