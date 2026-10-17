#ifndef WOWDECOMP_NATIVE_MODEL_ARENA_H
#define WOWDECOMP_NATIVE_MODEL_ARENA_H
#include <cstddef>
#include <cstdint>

namespace OsNative {
// Original M2 array wire width, native owner-scoped lifetime. Never a pointer
// truncation: encoding checks signed displacement and the entire target extent.
struct RelativeArray { uint32_t count; int32_t offset; };
class ModelArena {
public:
    explicit ModelArena(size_t capacity);
    ~ModelArena();
    ModelArena(const ModelArena&) = delete;
    ModelArena& operator=(const ModelArena&) = delete;
    void* Allocate(size_t bytes, size_t alignment);
    bool Contains(const void* pointer, size_t bytes) const;
    void Encode(RelativeArray* anchor, const void* target, uint32_t count, size_t elementSize) const;
    const void* Resolve(const RelativeArray* anchor, size_t elementSize) const;
private:
    void* base = nullptr;
    size_t capacity = 0, used = 0, committed = 0, page = 0;
};
}
#endif
