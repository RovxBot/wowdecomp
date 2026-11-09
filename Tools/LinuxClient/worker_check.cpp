// Actual full-client WowConnectionNet and Storm lifetimes. Wrappers only inject
// explicit failures or observe real pthread/poll operations in this executable.
#include "WoW/Source/WowServices/WowConnectionNet.h"
#include "Engine/Source/Storm/Source/Linux/SSync.h"
#include "Engine/Source/Storm/H/storm.h"
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <dirent.h>
#include <initializer_list>
#include <memory>
#include <new>
#include <poll.h>
#include <pthread.h>
#include <thread>
#include <type_traits>

static void check(bool value, int line) {
    if (!value) { std::fprintf(stderr, "worker check failed at %d\n", line); std::abort(); }
}
#define require(value) check((value), __LINE__)
using Worker = WowConnectionNet::WORKER;
#define MEMBER_ALIGNED(member) \
    static_assert(offsetof(Worker, member) % alignof(decltype(Worker::member)) == 0, "Packed member: " #member); \
    static_assert(alignof(Worker) >= alignof(decltype(Worker::member)), "Underaligned owner: " #member)
MEMBER_ALIGNED(m_owner);
MEMBER_ALIGNED(m_thread);
MEMBER_ALIGNED(m_index);
MEMBER_ALIGNED(m_connection);
MEMBER_ALIGNED(m_event);
MEMBER_ALIGNED(m_quit);
MEMBER_ALIGNED(m_lock);
MEMBER_ALIGNED(m_lastTime);
static_assert(sizeof(Worker) % alignof(Worker) == 0, "Array stride must retain alignment");
static_assert(offsetof(WowConnectionNet, m_workers) % alignof(Worker) == 0, "Containing array alignment");
static_assert(alignof(WowConnectionNet) >= alignof(Worker), "Containing allocation alignment");
static_assert(alignof(WowConnectionNet) <= alignof(std::max_align_t), "Reaudit SMem allocation");

static std::atomic<int> mutexes{0}, liveThreads{0}, callbacks{0}, pollReturns{0};
static std::atomic<bool> failThread{false}, failPipe{false};
static int failSyncAfter = -1;
extern "C" int __real_pthread_mutex_init(pthread_mutex_t*, const pthread_mutexattr_t*);
extern "C" int __real_pthread_mutex_destroy(pthread_mutex_t*);
extern "C" int __wrap_pthread_mutex_init(pthread_mutex_t* mutex, const pthread_mutexattr_t* attr) {
    require(reinterpret_cast<std::uintptr_t>(mutex) % alignof(pthread_mutex_t) == 0);
    const int result = __real_pthread_mutex_init(mutex, attr);
    if (!result) ++mutexes;
    return result;
}
extern "C" int __wrap_pthread_mutex_destroy(pthread_mutex_t* mutex) {
    require(reinterpret_cast<std::uintptr_t>(mutex) % alignof(pthread_mutex_t) == 0);
    const int result = __real_pthread_mutex_destroy(mutex);
    require(!result);
    --mutexes;
    return result;
}
extern "C" HANDLE __real__Z11SSyncCreate9SSYNCTYPEiii(SSYNCTYPE, int32_t, int32_t, int32_t);
extern "C" HANDLE __wrap__Z11SSyncCreate9SSYNCTYPEiii(SSYNCTYPE type, int32_t count, int32_t maximum, int32_t manual) {
    if (failSyncAfter >= 0 && failSyncAfter-- == 0) throw std::bad_alloc();
    return __real__Z11SSyncCreate9SSYNCTYPEiii(type, count, maximum, manual);
}
struct Launch { void* (*entry)(void*); void* parameter; };
static void* observedEntry(void* parameter) {
    auto launch = *static_cast<Launch*>(parameter);
    delete static_cast<Launch*>(parameter);
    void* result = launch.entry(launch.parameter);
    --liveThreads; // Observe the real detached entry's complete return.
    return result;
}
extern "C" int __real_pthread_create(pthread_t*, const pthread_attr_t*, void* (*)(void*), void*);
extern "C" int __wrap_pthread_create(pthread_t* thread, const pthread_attr_t* attr, void* (*entry)(void*), void* parameter) {
    if (failThread.exchange(false)) return EAGAIN;
    auto* launch = new Launch{entry, parameter};
    ++liveThreads;
    const int result = __real_pthread_create(thread, attr, observedEntry, launch);
    if (result) { --liveThreads; delete launch; }
    return result;
}
extern "C" int __real_pipe2(int*, int);
extern "C" int __wrap_pipe2(int* descriptors, int flags) {
    if (failPipe.exchange(false)) { errno = EMFILE; return -1; }
    return __real_pipe2(descriptors, flags);
}
extern "C" int __real_poll(pollfd*, nfds_t, int);
extern "C" int __wrap_poll(pollfd* descriptors, nfds_t count, int timeout) {
    const int result = __real_poll(descriptors, count, timeout);
    ++pollReturns;
    return result;
}
template<class Predicate> static void await(Predicate ready) {
    const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (!ready()) { require(std::chrono::steady_clock::now() < end); std::this_thread::yield(); }
}
static int descriptors() {
    auto* dir = opendir("/proc/self/fd"); require(dir);
    int count = 0;
    while (readdir(dir)) ++count;
    require(!closedir(dir)); return count;
}
template<class T> static void aligned(T& object) {
    require(reinterpret_cast<std::uintptr_t>(&object) % alignof(T) == 0);
}
static void inspect(Worker& worker) {
    aligned(worker); aligned(worker.m_owner); aligned(worker.m_thread);
    aligned(worker.m_index); aligned(worker.m_connection); aligned(worker.m_event);
    aligned(worker.m_lock); aligned(worker.m_lock.m_critsect); aligned(worker.m_lastTime);
    require(!worker.m_thread.Valid() && worker.m_event.Valid());
    require(worker.m_event.Wait(0) == WAIT_TIMEOUT);
    require(worker.m_event.Set() && worker.m_event.Wait(0) == WAIT_OBJECT_0);
    require(worker.m_event.Wait(0) == WAIT_TIMEOUT); // Original auto-reset semantics.
    worker.m_lock.Enter(); worker.m_lock.Enter();
    worker.m_lock.Leave(); worker.m_lock.Leave();
}
static uint32_t STORMAPI eventThread(void* pointer) {
    auto& worker = *static_cast<Worker*>(pointer);
    require(worker.m_event.Wait(2000) == WAIT_OBJECT_0);
    worker.m_lock.Enter(); ++worker.m_lastTime; worker.m_lock.Leave();
    return 0;
}
static void initialized() { ++callbacks; }
int main() {
    std::printf("worker layout: size=%zu align=%zu container=%zu array=%zu thread=%zu connection=%zu event=%zu lock=%zu last=%zu\n",
        sizeof(Worker), alignof(Worker), sizeof(WowConnectionNet), offsetof(WowConnectionNet, m_workers),
        offsetof(Worker, m_thread), offsetof(Worker, m_connection), offsetof(Worker, m_event),
        offsetof(Worker, m_lock), offsetof(Worker, m_lastTime));
    for (int repeat = 0; repeat < 4; ++repeat) {
        const int baseline = mutexes;
        {
            struct Embedded { char prefix; Worker array[33]; } embedded;
            auto array = std::make_unique<Worker[]>(33);
            for (int i = 0; i < 33; ++i) { inspect(embedded.array[i]); inspect(array[i]); }
        }
        require(mutexes == baseline);
        void* storage = SMemAlloc(sizeof(WowConnectionNet), __FILE__, __LINE__, 0);
        require(storage);
        auto* network = new (storage) WowConnectionNet(1, initialized);
        aligned(*network); aligned(network->m_stopEvent); aligned(network->m_connectionsLock);
        aligned(network->m_workerSemaphore); aligned(network->m_serviceSemaphore);
        for (auto& worker : network->m_workers) inspect(worker);
        network->~WowConnectionNet();
        SMemFree(storage, __FILE__, __LINE__, 0);
        require(mutexes == baseline);
    }
    // Every throwing event/semaphore-construction edge unwinds the actual
    // constructed worker prefix and containing members; no thread has started.
    const int baseline = mutexes;
    for (int failure = 0; failure < 35; ++failure) {
        failSyncAfter = failure;
        try { auto network = std::make_unique<WowConnectionNet>(1, initialized); require(false); }
        catch (const std::bad_alloc&) { require(failSyncAfter == -1); }
        require(mutexes == baseline && liveThreads == 0);
    }
    for (int failure : {0, 1, 16, 32}) {
        failSyncAfter = failure;
        try { auto array = std::make_unique<Worker[]>(33); require(false); }
        catch (const std::bad_alloc&) { require(failSyncAfter == -1); }
        require(mutexes == baseline);
    }
    {
        Worker worker;
        worker.m_lastTime = 0;
        char name[] = "worker fixture";
        failThread = true;
        const int before = mutexes;
        require(!SThread::Create(eventThread, &worker, worker.m_thread, name, 0));
        require(!worker.m_thread.Valid() && !failThread && liveThreads == 0);
        // SCreateThread lazily constructs its persistent tracking lock once.
        require(mutexes == before+1);
        require(SThread::Create(eventThread, &worker, worker.m_thread, name, 0));
        require(worker.m_event.Set() && worker.m_thread.Wait(3000) == WAIT_OBJECT_0);
        await([] { return liveThreads == 0; });
        require(worker.m_lastTime == 1);
    }
    require(mutexes == baseline+1);
    // Original Start/RunWorker/Stop, thread-init callback, auto-reset wakeups,
    // POSIX poll/wake pipe and PlatformDestroy, including a failed pipe setup.
    for (bool pipeFailure : {false, true}) for (int workers : {0, 1, 4, 32}) {
        const int before = mutexes, fds = descriptors(), oldCallbacks = callbacks;
        {
            auto network = std::make_unique<WowConnectionNet>(workers, initialized);
            failPipe = pipeFailure;
            network->PlatformInit(false);
            require(!failPipe && descriptors() == fds + (pipeFailure ? 0 : 2));
            network->Start();
            await([&] { return callbacks == oldCallbacks+workers; });
            require(network->m_thread.Valid());
            for (int i = 0; i < workers; ++i) require(network->m_workers[i].m_thread.Valid());
            const int polls = pollReturns;
            network->PlatformWorkerReady();
            await([&] { return pollReturns > polls; });
            network->Stop();
            await([] { return liveThreads == 0; });
            require(network->m_stopEvent.Wait(0) == WAIT_OBJECT_0);
            for (int i = 0; i < workers; ++i) {
                require(network->m_workers[i].m_quit && !network->m_workers[i].m_connection);
                require(network->m_workers[i].m_thread.Wait(0) == WAIT_OBJECT_0);
            }
            network->PlatformDestroy();
            require(!network->m_event && descriptors() == fds);
        }
        require(mutexes == before && liveThreads == 0 && descriptors() == fds);
    }
    std::puts("native-network-worker=passed: actual aligned members/arrays/SMem container, event/lock/thread lifetimes, construction unwind, thread/pipe failure and original network start/wake/stop/destruction");
}
