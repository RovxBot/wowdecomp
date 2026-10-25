#include "ModelMemory.h"
#include "ModelArena.h"
#include "Engine/Source/Os/OsMemory.h"
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <stdexcept>

// Native implementation, not the Win64 OsAllocNear allocator. Every returned
// payload belongs to one retained reservation, including the original root.
namespace {
struct Group {
    OsNative::ModelArena arena;
    std::thread::id owner = std::this_thread::get_id();
    bool retired = false;
    explicit Group(size_t capacity) : arena(capacity) {}
};
struct Allocation {
    std::shared_ptr<Group> group;
    size_t bytes;
    bool root;
};
struct Registry {
    std::mutex mutex;
    std::map<uintptr_t, Allocation> allocations;
};
Registry& Records() { static Registry records; return records; }
auto Find(Registry& records, const void* anchor) {
    auto address = reinterpret_cast<uintptr_t>(anchor);
    auto i = records.allocations.upper_bound(address);
    if (i == records.allocations.begin()) return records.allocations.end();
    --i;
    return address - i->first < i->second.bytes ? i : records.allocations.end();
}
void Free(void* pointer, bool root) {
    if (!pointer) return;
    auto& records = Records();
    std::lock_guard<std::mutex> lock(records.mutex);
    auto i = records.allocations.find(reinterpret_cast<uintptr_t>(pointer));
    if (i == records.allocations.end() || i->second.root != root ||
        i->second.group->owner != std::this_thread::get_id())
        throw std::invalid_argument("Invalid native model allocation release");
    if (root) i->second.group->retired = true;
    // Outstanding cancelled-read payloads can retain the reservation after the
    // root is retired. They cannot allocate more arrays or expose reused roots.
    records.allocations.erase(i);
}
}
void* OsNative::AllocModelRoot(size_t bytes, size_t capacity) {
    if (!bytes || bytes > capacity) return nullptr;
    try {
        auto group = std::make_shared<Group>(capacity);
        void* result = group->arena.Allocate(bytes, 16);
        auto& records = Records();
        std::lock_guard<std::mutex> lock(records.mutex);
        records.allocations.emplace(reinterpret_cast<uintptr_t>(result), Allocation{group, bytes, true});
        return result;
    } catch (const std::bad_alloc&) { return nullptr; }
      catch (const std::runtime_error&) { return nullptr; }
}
void OsNative::FreeModelRoot(void* pointer) { Free(pointer, true); }
size_t OsNative::LiveModelAllocations() {
    auto& records = Records();
    std::lock_guard<std::mutex> lock(records.mutex);
    return records.allocations.size();
}
void* OsAllocNear(const void* anchor, size_t bytes) {
    if (!anchor || !bytes) return nullptr;
    auto& records = Records();
    std::lock_guard<std::mutex> lock(records.mutex);
    auto i = Find(records, anchor);
    if (i == records.allocations.end()) return nullptr;
    auto group = i->second.group;
    if (group->retired || group->owner != std::this_thread::get_id()) return nullptr;
    try {
        void* result = group->arena.Allocate(bytes, 16);
        // ModelArena caps the entire reservation at 1 GiB. Verify the complete
        // target too, before publishing an allocation to a relative-array caller.
        auto a = reinterpret_cast<uintptr_t>(anchor), b = reinterpret_cast<uintptr_t>(result);
        if (!group->arena.Contains(result, bytes) || b < a || b - a > INT32_MAX ||
            bytes - 1 > size_t(INT32_MAX) - (b - a)) return nullptr;
        records.allocations.emplace(b, Allocation{group, bytes, false});
        return result;
    } catch (const std::bad_alloc&) { return nullptr; }
      catch (const std::runtime_error&) { return nullptr; }
}
void OsFreeNear(void* pointer) { Free(pointer, false); }
