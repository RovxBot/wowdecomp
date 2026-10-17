#include "Engine/Source/Os/Portable/Runtime.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace {
enum class Kind { Event, Mutex, Semaphore, Thread };
struct State {
    Kind kind;
    bool manual = false, signaled = false, closed = false, abandoned = false;
    uint32_t count = 0, maximum = 0, recursion = 0;
    uint32_t id = 0;
    char name[16] = {};
    std::thread::id owner;
    explicit State(Kind value) : kind(value) {}
};
struct Registry {
    std::mutex mutex;
    std::condition_variable changed;
    std::vector<std::weak_ptr<State>> mutexes;
    std::vector<std::weak_ptr<State>> threadNames;
    std::atomic<size_t> threads{0};
};
Registry& RegistryInstance() { static Registry value; return value; }
std::atomic<uint32_t> nextThreadId{1};
thread_local uint32_t currentThreadId = nextThreadId.fetch_add(1);
thread_local const char* currentThreadName = "external";

struct ExitGuard {
    std::shared_ptr<State> completion;
    std::vector<std::pair<void (*)(void*), void*>> callbacks;
    ~ExitGuard() {
        // Destroy runtime TLS while the thread is still alive and owns its locks.
        OsRunTLSDestructors();
        for (auto it = callbacks.rbegin(); it != callbacks.rend(); ++it) it->first(it->second);
        auto& registry = RegistryInstance();
        std::lock_guard<std::mutex> lock(registry.mutex);
        for (auto it = registry.mutexes.begin(); it != registry.mutexes.end();) {
            auto state = it->lock();
            if (!state) { it = registry.mutexes.erase(it); continue; }
            if (state->owner == std::this_thread::get_id() && state->recursion) {
                state->owner = {};
                state->recursion = 0;
                state->abandoned = true;
            }
            ++it;
        }
        if (completion) {
            completion->signaled = true;
            registry.threads.fetch_sub(1);
        }
        registry.changed.notify_all();
    }
};
ExitGuard& ThreadExit() { thread_local ExitGuard guard; return guard; }

bool Ready(const State& state) {
    if (state.kind == Kind::Mutex)
        return !state.recursion || state.owner == std::this_thread::get_id();
    if (state.kind == Kind::Semaphore) return state.count != 0;
    return state.signaled;
}
bool Acquire(State& state) {
    bool abandoned = state.abandoned;
    if (state.kind == Kind::Mutex) {
        state.owner = std::this_thread::get_id();
        ++state.recursion;
        state.abandoned = false;
    } else if (state.kind == Kind::Semaphore) --state.count;
    else if (state.kind == Kind::Event && !state.manual) state.signaled = false;
    return abandoned;
}
}

struct OsSyncHandle { std::shared_ptr<State> state; };

OsSyncHandle* OsCreateEvent(bool manualReset, bool initialState) {
    auto state = std::make_shared<State>(Kind::Event);
    state->manual = manualReset;
    state->signaled = initialState;
    return new OsSyncHandle{std::move(state)};
}
OsSyncHandle* OsCreateMutex(bool initialOwner, const char* name) {
    // Cross-process named mutexes have no portable equivalent in this runtime.
    if (name && *name) return nullptr;
    auto state = std::make_shared<State>(Kind::Mutex);
    ThreadExit();
    if (initialOwner) { state->owner = std::this_thread::get_id(); state->recursion = 1; }
    auto& registry = RegistryInstance();
    std::lock_guard<std::mutex> lock(registry.mutex);
    registry.mutexes.emplace_back(state);
    return new OsSyncHandle{std::move(state)};
}
OsSyncHandle* OsCreateSemaphore(uint32_t initialCount, uint32_t maximumCount) {
    if (!maximumCount || initialCount > maximumCount) return nullptr;
    auto state = std::make_shared<State>(Kind::Semaphore);
    state->count = initialCount;
    state->maximum = maximumCount;
    return new OsSyncHandle{std::move(state)};
}
void OsCloseSync(OsSyncHandle* handle) {
    if (!handle) return;
    auto& registry = RegistryInstance();
    {
        std::lock_guard<std::mutex> lock(registry.mutex);
        handle->state->closed = true;
        registry.changed.notify_all();
    }
    delete handle;
}
bool OsSetEvent(OsSyncHandle* handle, bool signaled) {
    if (!handle) return false;
    auto& registry = RegistryInstance();
    std::lock_guard<std::mutex> lock(registry.mutex);
    auto& state = *handle->state;
    if (state.closed || state.kind != Kind::Event) return false;
    state.signaled = signaled;
    if (signaled) registry.changed.notify_all();
    return true;
}
bool OsReleaseMutex(OsSyncHandle* handle) {
    if (!handle) return false;
    auto& registry = RegistryInstance();
    std::lock_guard<std::mutex> lock(registry.mutex);
    auto& state = *handle->state;
    if (state.closed || state.kind != Kind::Mutex || !state.recursion ||
        state.owner != std::this_thread::get_id()) return false;
    if (!--state.recursion) { state.owner = {}; registry.changed.notify_all(); }
    return true;
}
bool OsReleaseSemaphore(OsSyncHandle* handle, uint32_t count) {
    if (!handle || !count) return false;
    auto& registry = RegistryInstance();
    std::lock_guard<std::mutex> lock(registry.mutex);
    auto& state = *handle->state;
    if (state.closed || state.kind != Kind::Semaphore || count > state.maximum - state.count) return false;
    state.count += count;
    registry.changed.notify_all();
    return true;
}
uint32_t OsWaitObjects(uint32_t count, OsSyncHandle* const* handles, bool waitAll, uint32_t timeout) {
    if (!count || count > 64 || !handles) return OS_WAIT_FAILED;
    // Register cleanup before acquiring any mutex, so owner death is observable.
    ThreadExit();
    auto& registry = RegistryInstance();
    std::unique_lock<std::mutex> lock(registry.mutex);
    std::array<std::shared_ptr<State>, 64> states;
    for (uint32_t i = 0; i < count; ++i) {
        if (!handles[i]) return OS_WAIT_FAILED;
        states[i] = handles[i]->state;
        for (uint32_t j = 0; j < i; ++j)
            if (states[i] == states[j]) return OS_WAIT_FAILED;
    }
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout);
    for (;;) {
        for (uint32_t i = 0; i < count; ++i) if (states[i]->closed) return OS_WAIT_FAILED;
        if (waitAll) {
            bool ready = true;
            for (uint32_t i = 0; i < count; ++i) ready &= Ready(*states[i]);
            if (ready) {
                bool abandoned = false;
                for (uint32_t i = 0; i < count; ++i) abandoned |= Acquire(*states[i]);
                return abandoned ? OS_WAIT_ABANDONED : 0;
            }
        } else {
            for (uint32_t i = 0; i < count; ++i)
                if (Ready(*states[i])) return (Acquire(*states[i]) ? OS_WAIT_ABANDONED : 0) + i;
        }
        if (!timeout) return OS_WAIT_TIMEOUT;
        if (timeout == OS_WAIT_INFINITE) registry.changed.wait(lock);
        else if (registry.changed.wait_until(lock, deadline) == std::cv_status::timeout) {
            // Recheck predicates under the lock at the deadline before timing out.
            timeout = 0;
        }
    }
}

namespace {
struct Start { OsThreadProc function; void* argument; std::shared_ptr<State> state; uint32_t id; };
void ThreadStart(void* parameter) {
    std::unique_ptr<Start> start(static_cast<Start*>(parameter));
    ThreadExit().completion = start->state;
    currentThreadId = start->id;
    currentThreadName = start->state->name;
    start->function(start->argument);
}
}
OsSyncHandle* OsCreateThread(OsThreadProc function, void* argument, uint32_t stackSize,
                             uint32_t* threadId, const char* name) {
    if (!function) return nullptr;
    auto state = std::make_shared<State>(Kind::Thread);
    state->manual = true;
    const uint32_t id = nextThreadId.fetch_add(1);
    state->id = id;
    std::strncpy(state->name, name ? name : "worker", sizeof(state->name) - 1);
    {
        auto& registry = RegistryInstance();
        std::lock_guard<std::mutex> lock(registry.mutex);
        registry.threadNames.erase(std::remove_if(registry.threadNames.begin(), registry.threadNames.end(),
            [](const auto& value) { return value.expired(); }), registry.threadNames.end());
        registry.threadNames.emplace_back(state);
    }
    auto start = std::make_unique<Start>(Start{function, argument, state, id});
    auto handle = std::make_unique<OsSyncHandle>(OsSyncHandle{state});
    RegistryInstance().threads.fetch_add(1);
    if (!OsLaunchThread(ThreadStart, start.get(), stackSize ? stackSize : 8 * 1024 * 1024)) {
        RegistryInstance().threads.fetch_sub(1);
        return nullptr;
    }
    start.release();
    // IDs are process-local runtime identities, not native kernel IDs.
    if (threadId) *threadId = id;
    return handle.release();
}
uint32_t OsCurrentThreadId() { return currentThreadId; }
bool OsLookupThreadName(uint32_t id, char* buffer, size_t size) {
    if (!buffer || !size) return false;
    buffer[0] = 0;
    auto copy = [&](const char* name) { std::strncpy(buffer, name, size - 1); buffer[size - 1] = 0; return true; };
    if (id == currentThreadId) return copy(currentThreadName);
    auto& registry = RegistryInstance();
    std::lock_guard<std::mutex> lock(registry.mutex);
    for (const auto& weak : registry.threadNames)
        if (auto state = weak.lock(); state && state->id == id) return copy(state->name);
    return false;
}
void OsEnsureThreadExit() { ThreadExit(); }
void OsAtThreadExit(void (*callback)(void*), void* argument) {
    ThreadExit().callbacks.emplace_back(callback, argument);
}
size_t OsLiveThreadCount() { return RegistryInstance().threads.load(); }
