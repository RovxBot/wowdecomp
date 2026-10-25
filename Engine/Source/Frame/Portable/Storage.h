#ifndef WOWDECOMP_NATIVE_FRAME_STORAGE_H
#define WOWDECOMP_NATIVE_FRAME_STORAGE_H
#include "Engine/Source/Base/CDataAllocator.h"
#include "Runtime.h"
#include <utility>

namespace Frame { namespace Native {
// Pools are borrowed static class allocators, registered with the Fonts owner.
// Selected factories validate tree mutation before constructing their objects.
void* AllocateRegionStorage(CDataAllocator&, size_t bytes, size_t alignment);
// Class delete accepts both selected pool slots and ordinary C++ heap objects.
void FreeRegionStorage(CDataAllocator&, void*);
template<class T, class... Args>
T* ConstructRegion(CDataAllocator& pool, Args&&... args) {
    // Callback callers require the checked factory's construction capability.
    // It covers failure cleanup, but is suspended during every nested callback.
    RequireDestroyable();
    void* storage = AllocateRegionStorage(pool, sizeof(T), alignof(T));
    try {
        return ::new (storage) T(std::forward<Args>(args)...);
    } catch (...) {
        FreeRegionStorage(pool, storage);
        throw;
    }
}
}}
#endif
