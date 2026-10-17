#include "Engine/Source/Os/Portable/Runtime.h"
#include "Engine/Source/Os/Tls.h"
#include <array>
#include <mutex>
#include <vector>

namespace {
struct Slot { uint32_t generation = 0; OsTLSDestructor destructor = nullptr; bool used = false; };
struct Slots { std::mutex mutex; std::array<Slot, 1024> slots; };
Slots& Registry() { static Slots slots; return slots; }
struct Value { uint32_t key; void* value; OsTLSDestructor destructor; };
// Owned by OsRunTLSDestructors, so destructor ordering cannot invalidate storage.
thread_local std::vector<Value>* values = nullptr;
bool Valid(uint32_t key, OsTLSDestructor* destructor = nullptr) {
    auto& registry = Registry();
    std::lock_guard<std::mutex> lock(registry.mutex);
    const auto& slot = registry.slots[key & 1023];
    if (!slot.used || slot.generation != key / 1024) return false;
    if (destructor) *destructor = slot.destructor;
    return true;
}
}
uint32_t OsAllocateTLS(OsTLSDestructor destructor) {
    OsEnsureThreadExit();
    auto& registry = Registry();
    std::lock_guard<std::mutex> lock(registry.mutex);
    for (uint32_t i = 0; i < registry.slots.size(); ++i) {
        auto& slot = registry.slots[i];
        // Never reuse an identity after generation wrap.
        if (!slot.used && slot.generation < 0x3FFFFF) {
            ++slot.generation;
            slot.used = true;
            slot.destructor = destructor;
            return slot.generation * 1024 + i;
        }
    }
    return UINT32_MAX;
}
bool OsFreeTLS(uint32_t key) {
    auto& registry = Registry();
    std::lock_guard<std::mutex> lock(registry.mutex);
    auto& slot = registry.slots[key & 1023];
    if (!slot.used || slot.generation != key / 1024) return false;
    slot.used = false;
    slot.destructor = nullptr;
    return true;
}
void* OsGetTLS(uint32_t key) {
    OsEnsureThreadExit();
    if (!Valid(key) || !values) return nullptr;
    for (auto& value : *values) if (value.key == key) return value.value;
    return nullptr;
}
bool OsSetTLS(uint32_t key, void* pointer) {
    OsEnsureThreadExit();
    OsTLSDestructor destructor;
    if (!Valid(key, &destructor)) return false;
    if (!values) values = new std::vector<Value>;
    for (auto& value : *values) {
        if (value.key == key) { value.value = pointer; value.destructor = destructor; return true; }
    }
    values->push_back({key, pointer, destructor});
    return true;
}
void OsRunTLSDestructors() {
    if (!values) return;
    // POSIX-style bounded destructor re-registration. Clear before invoking user
    // code, copy the entry, and tolerate callbacks growing/reallocating the vector.
    for (int pass = 0; pass < 4; ++pass) {
        bool called = false;
        const size_t count = values->size();
        for (size_t i = 0; i < count; ++i) {
            Value value = (*values)[i];
            if (!value.value || !value.destructor || !Valid(value.key)) continue;
            (*values)[i].value = nullptr;
            value.destructor(value.value);
            called = true;
        }
        if (!called) break;
    }
    delete values;
    values = nullptr;
}
uint32_t OsTlsAlloc() { return OsAllocateTLS(nullptr); }
void OsTlsFree(uint32_t index) { OsFreeTLS(index); }
void* OsTlsGetValue(uint32_t index) { return OsGetTLS(index); }
int32_t OsTlsSetValue(uint32_t index, void* value) { return OsSetTLS(index, value); }
