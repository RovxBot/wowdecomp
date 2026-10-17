#include "Engine/Source/Storm/H/storm.h"
#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <limits>
#include <new>

// Native adaptation of the 12340 SMem contract. No 12340 address/VERIFY claim:
// capacity is recorded explicitly instead of querying a Windows CRT heap.
namespace {
struct alignas(std::max_align_t) Header { void* allocation; size_t capacity; size_t alignment; };
std::atomic<bool> exitOnFailure{true};
void* Failure(size_t bytes, const char* file, int32_t line) {
    char message[128];
    std::snprintf(message, sizeof(message), "Requested %zu bytes of memory", bytes);
    SErrDisplayError(STORM_ERROR_NOT_ENOUGH_MEMORY, file, line, message, !exitOnFailure.load(), 1, 0);
    return nullptr;
}
void* Allocate(size_t bytes, size_t alignment, bool zero, const char* file, int32_t line) {
    const size_t limit = std::numeric_limits<size_t>::max();
    if (bytes > limit - 7) return Failure(bytes, file, line);
    size_t capacity = std::max<size_t>((bytes + 7) & ~size_t(7), 8);
    if (alignment < alignof(Header)) alignment = alignof(Header);
    if (capacity > limit - sizeof(Header) - (alignment - 1)) return Failure(bytes, file, line);
    void* base = std::malloc(capacity + sizeof(Header) + alignment - 1);
    if (!base) return Failure(bytes, file, line);
    uintptr_t pointer = (reinterpret_cast<uintptr_t>(base) + sizeof(Header) + alignment - 1) & ~(alignment - 1);
    auto header = reinterpret_cast<Header*>(pointer) - 1;
    header->allocation = base;
    header->capacity = capacity;
    header->alignment = alignment;
    if (zero) std::memset(reinterpret_cast<void*>(pointer), 0, capacity);
    return reinterpret_cast<void*>(pointer);
}
}
void* STORMAPI SMemAlloc(size_t bytes, const char* file, int32_t line, uint32_t flags) {
    return Allocate(bytes, alignof(std::max_align_t), flags & SMEM_FLAG_ZEROMEMORY, file, line);
}
int32_t STORMAPI SMemFree(void* pointer, const char*, int32_t, uint32_t) {
    if (pointer) std::free((static_cast<Header*>(pointer) - 1)->allocation);
    return 1;
}
void* STORMAPI SMemReAlloc(void* pointer, size_t bytes, const char* file, int32_t line, uint32_t flags) {
    if (flags == SMEM_REALLOC_NULL) return nullptr;
    if (!pointer) return SMemAlloc(bytes, file, line, flags);
    if (flags & SMEM_FLAG_NOREALLOC) return nullptr;
    if (!bytes) { SMemFree(pointer, file, line, 0); return nullptr; }
    const auto old = *(static_cast<Header*>(pointer) - 1);
    void* result = Allocate(bytes, old.alignment, false, file, line);
    if (!result) return nullptr; // Preserve the old allocation on failure.
    std::memcpy(result, pointer, std::min(bytes, old.capacity));
    if ((flags & SMEM_FLAG_ZEROMEMORY) && bytes > old.capacity)
        std::memset(static_cast<uint8_t*>(result) + old.capacity, 0, bytes - old.capacity);
    SMemFree(pointer, file, line, 0);
    return result;
}
void STORMAPI SMemSetDebugFlags(uint32_t flags, uint8_t mask) {
    if (mask & SMEM_DEBUG_NOEXITONFAILURE) exitOnFailure.store(!(flags & SMEM_DEBUG_NOEXITONFAILURE));
}
void STORMAPI SMemInitialize() { /* Allocator is valid before CRT constructors. */ }
int32_t STORMAPI SMemDestroy() { return 1; } // Live allocations retain their owner through static teardown.

void* operator new(size_t bytes) {
    if (void* result = SMemAlloc(bytes, "new", 0, 0)) return result;
    throw std::bad_alloc();
}
void* operator new[](size_t bytes) { return ::operator new(bytes); }
void operator delete(void* pointer) noexcept { SMemFree(pointer, "delete", 0, 0); }
void operator delete[](void* pointer) noexcept { ::operator delete(pointer); }
void operator delete(void* pointer, size_t) noexcept { ::operator delete(pointer); }
void operator delete[](void* pointer, size_t) noexcept { ::operator delete(pointer); }
void* operator new(size_t bytes, std::align_val_t alignment) {
    if (void* result = Allocate(bytes, static_cast<size_t>(alignment), false, "aligned new", 0)) return result;
    throw std::bad_alloc();
}
void* operator new[](size_t bytes, std::align_val_t alignment) { return ::operator new(bytes, alignment); }
void operator delete(void* pointer, std::align_val_t) noexcept { ::operator delete(pointer); }
void operator delete[](void* pointer, std::align_val_t) noexcept { ::operator delete(pointer); }
void operator delete(void* pointer, size_t, std::align_val_t) noexcept { ::operator delete(pointer); }
void operator delete[](void* pointer, size_t, std::align_val_t) noexcept { ::operator delete(pointer); }
void* operator new(size_t bytes, const std::nothrow_t&) noexcept {
    try { return ::operator new(bytes); } catch (...) { return nullptr; }
}
void* operator new[](size_t bytes, const std::nothrow_t& tag) noexcept { return ::operator new(bytes, tag); }
void operator delete(void* pointer, const std::nothrow_t&) noexcept { ::operator delete(pointer); }
void operator delete[](void* pointer, const std::nothrow_t&) noexcept { ::operator delete(pointer); }
void* operator new(size_t bytes, std::align_val_t alignment, const std::nothrow_t&) noexcept {
    try { return ::operator new(bytes, alignment); } catch (...) { return nullptr; }
}
void* operator new[](size_t bytes, std::align_val_t alignment, const std::nothrow_t& tag) noexcept {
    return ::operator new(bytes, alignment, tag);
}
void operator delete(void* pointer, std::align_val_t, const std::nothrow_t&) noexcept { ::operator delete(pointer); }
void operator delete[](void* pointer, std::align_val_t, const std::nothrow_t&) noexcept { ::operator delete(pointer); }
