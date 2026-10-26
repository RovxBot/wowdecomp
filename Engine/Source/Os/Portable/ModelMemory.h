#ifndef WOWDECOMP_NATIVE_MODEL_MEMORY_H
#define WOWDECOMP_NATIVE_MODEL_MEMORY_H
#include <cstddef>

namespace OsNative {
// Reserve the model root and its external arrays together. Allocation failures
// return null; there is no fallback to an unrelated heap or a fixed mapping.
void* AllocModelRoot(size_t bytes, size_t capacity = 256 * 1024 * 1024);
void FreeModelRoot(void*);
size_t LiveModelAllocations();
}
#endif
