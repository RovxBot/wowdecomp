#include "Engine/Source/Os/OsMemory.h"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>

static void Check(bool condition) { if (!condition) std::abort(); }
int main() {
    void* anchor = std::malloc(4096);
    Check(anchor);
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
    std::free(anchor);
}
