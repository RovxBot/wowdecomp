#include "Engine/Source/Storm/H/storm.h"
#include "Engine/Source/Storm/Source/SThread.h"
#include "Engine/Source/Os/OsSecureRandom.h"
#include "Engine/Source/Os/Tls.h"
#include "Engine/Source/Os/Time.h"
#include "Engine/Source/BlizzardCore/Source/System/Lock/Lock.h"
#include "Engine/Source/BlizzardCore/Source/System/Memory/MemoryStorm.h"
#include "Engine/Source/BlizzardCore/Source/System/Thread/ThreadLocalStorage.h"
#include "Engine/Source/BlizzardCore/Source/System/Thread/Thread.h"
#include "Engine/Source/BlizzardCore/Source/System/Time/Time.h"
#include <array>
#include <atomic>
#include <chrono>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <limits>
#include <new>
#include <string>
#include <thread>
#include <vector>

#define CHECK(condition) do { if (!(condition)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); std::abort(); } } while (0)
std::atomic<int> destroyed{0};
struct OnceValue { static std::atomic<int> constructions; int value = 42; OnceValue() { ++constructions; } };
std::atomic<int> OnceValue::constructions{0};
struct Local { ~Local() { ++destroyed; } };
Blizzard::Thread::LocalObject<Local> localObject{};
uint32_t tlsKey;
uint32_t ThreadTLS(void*) {
    char name[16];
    CHECK(SGetThreadName(SGetCurrentThreadId(), name, sizeof(name)) && name[0]);
    CHECK(!OsTlsGetValue(tlsKey));
    int value = 123;
    CHECK(OsTlsSetValue(tlsKey, &value));
    CHECK(OsTlsGetValue(tlsKey) == &value);
    Local& local = localObject;
    (void)local;
    SErrSetLastError(456);
    CHECK(SErrGetLastError() == 456);
    return 0;
}
void Allocation() {
    auto memory = static_cast<uint8_t*>(SMemAlloc(9, __FILE__, __LINE__, SMEM_FLAG_ZEROMEMORY));
    CHECK(memory && reinterpret_cast<uintptr_t>(memory) % alignof(std::max_align_t) == 0);
    for (int i = 0; i < 16; ++i) CHECK(memory[i] == 0);
    std::memset(memory, 42, 16);
    CHECK(!SMemReAlloc(memory, 40, __FILE__, __LINE__, SMEM_FLAG_NOREALLOC));
    CHECK(!SMemReAlloc(memory, 40, __FILE__, __LINE__, SMEM_REALLOC_NULL));
    auto grown = static_cast<uint8_t*>(SMemReAlloc(memory, 40, __FILE__, __LINE__, SMEM_FLAG_ZEROMEMORY));
    CHECK(grown);
    for (int i = 0; i < 40; ++i) CHECK(grown[i] == (i < 16 ? 42 : 0));
    SMemSetDebugFlags(SMEM_DEBUG_NOEXITONFAILURE, SMEM_DEBUG_NOEXITONFAILURE);
    CHECK(!SMemReAlloc(grown, SIZE_MAX, __FILE__, __LINE__, 0));
    CHECK(grown[0] == 42);
    CHECK(!SMemAlloc(SIZE_MAX, __FILE__, __LINE__, 0));
    CHECK(SErrGetLastError() == STORM_ERROR_NOT_ENOUGH_MEMORY);
    bool stormThrew = false, blizzardThrew = false;
    try { ::operator new(SIZE_MAX, __FILE__, __LINE__); }
    catch (const std::bad_alloc&) { stormThrew = true; }
    try { ::operator new(SIZE_MAX, uint32_t(0), __FILE__, __LINE__); }
    catch (const std::bad_alloc&) { blizzardThrew = true; }
    CHECK(stormThrew && blizzardThrew);
    SMemSetDebugFlags(0, SMEM_DEBUG_NOEXITONFAILURE);
    CHECK(!SMemReAlloc(grown, 0, __FILE__, __LINE__, 0));
    struct alignas(256) Aligned { char data[513]; };
    auto aligned = new Aligned[2];
    CHECK(reinterpret_cast<uintptr_t>(aligned) % 256 == 0);
    delete[] aligned;
    auto data = static_cast<uint8_t*>(Blizzard::Memory::Allocate(17, 1, __FILE__, __LINE__));
    CHECK(data && data[0] == 0 && data[16] == 0);
    Blizzard::Memory::Free(data);
}
void WaitSemantics() {
    SEvent automatic(0, 1), manual(1, 1), unavailable(0, 0);
    CHECK(automatic.Wait(0) == 0 && automatic.Wait(0) == OS_WAIT_TIMEOUT);
    CHECK(manual.Wait(0) == 0 && manual.Wait(0) == 0);
    CHECK(manual.Reset() && manual.Wait(0) == OS_WAIT_TIMEOUT);
    SSemaphore semaphore(1, 2);
    SSyncObject* all[] = {&semaphore, &unavailable};
    CHECK(SSyncObject::WaitMultiplePtr(2, all, 1, 0) == OS_WAIT_TIMEOUT);
    CHECK(semaphore.Wait(0) == 0); // Failed wait-all did not consume its token.
    CHECK(semaphore.Signal(1));
    unavailable.Set();
    CHECK(SSyncObject::WaitMultiplePtr(2, all, 1, 0) == 0);
    CHECK(unavailable.Wait(0) == OS_WAIT_TIMEOUT);
    CHECK(semaphore.Wait(0) == OS_WAIT_TIMEOUT);
    CHECK(semaphore.Signal(2) && !semaphore.Signal(1));
    CHECK(semaphore.Wait(0) == 0 && semaphore.Wait(0) == 0);
    SSyncObject* any[] = {&unavailable, &manual};
    manual.Set();
    CHECK(SSyncObject::WaitMultiplePtr(2, any, 0, 0) == 1);
    unavailable.Set();
    CHECK(SSyncObject::WaitMultiplePtr(2, any, 0, 0) == 0);
    SSyncObject* duplicate[] = {&manual, &manual};
    CHECK(SSyncObject::WaitMultiplePtr(2, duplicate, 1, 0) == OS_WAIT_FAILED);
    CHECK(SSyncObject::WaitMultiplePtr(0, any, 0, 0) == OS_WAIT_FAILED);
    const auto start = std::chrono::steady_clock::now();
    CHECK(unavailable.Wait(25) == OS_WAIT_TIMEOUT);
    CHECK(std::chrono::steady_clock::now() - start >= std::chrono::milliseconds(20));
    SMutex mutex;
    CHECK(mutex.Wait(0) == 0 && mutex.Wait(0) == 0);
    std::thread foreign([&] { CHECK(!mutex.Release()); CHECK(mutex.Wait(0) == OS_WAIT_TIMEOUT); });
    foreign.join();
    CHECK(mutex.Release() && mutex.Release() && !mutex.Release());
    std::thread owner([&] { CHECK(mutex.Wait(0) == 0); });
    owner.join();
    CHECK(mutex.Wait(1000) == OS_WAIT_ABANDONED);
    CHECK(mutex.Release());
    SMutex named;
    named.Create(0, "unsupported-cross-process-mutex");
    CHECK(!named.Valid());
    // Two waiters consume one auto-reset notification at a time.
    SEvent wake(0, 0);
    std::atomic<int> ready{0}, awakened{0};
    auto waiter = [&] { ++ready; CHECK(wake.Wait(2000) == 0); ++awakened; };
    std::thread first(waiter), second(waiter);
    while (ready.load() != 2) std::this_thread::yield();
    wake.Set();
    while (!awakened.load()) std::this_thread::yield();
    CHECK(awakened.load() == 1);
    wake.Set(); first.join(); second.join(); CHECK(awakened.load() == 2);
}
void LocksAndTLS() {
    tlsKey = OsTlsAlloc(); CHECK(tlsKey != UINT32_MAX);
    int mainValue = 1;
    CHECK(OsTlsSetValue(tlsKey, &mainValue));
    SErrSetLastError(123);
    std::array<SThread, 8> threads;
    for (auto& thread : threads) CHECK(SThread::Create(ThreadTLS, nullptr, thread, nullptr, 0));
    for (auto& thread : threads) CHECK(thread.Wait(5000) == 0);
    CHECK(destroyed.load() == 8); // Completion includes registered TLS destruction.
    CHECK(OsTlsGetValue(tlsKey) == &mainValue && SErrGetLastError() == 123);
    OsTlsFree(tlsKey);
    CHECK(!OsTlsSetValue(tlsKey, &mainValue));
    auto replacement = OsTlsAlloc();
    CHECK(replacement != tlsKey && !OsTlsGetValue(replacement));
    tlsKey = replacement;
    SCritSect critical;
    critical.Enter(); critical.Enter(); critical.Leave(); critical.Leave();
    Blizzard::Lock::RMutex recursive;
    Blizzard::Lock::MutexEnter(recursive.m_mutex);
    { Blizzard::Lock::ScopedMutex nested(recursive.m_mutex); }
    Blizzard::Lock::MutexLeave(recursive.m_mutex);
    Blizzard::Lock::Atomic atomic;
    std::vector<std::thread> workers;
    for (int i = 0; i < 8; ++i) workers.emplace_back([&] { for (int j = 0; j < 2000; ++j) atomic.Increment(); });
    for (auto& thread : workers) thread.join();
    CHECK(atomic.m_value.load() == 16000);
    atomic.m_value.store(INT32_MAX);
    CHECK(atomic.Increment() == INT32_MIN && atomic.Decrement() == INT32_MAX);
    Blizzard::Lock::ConstructOnce<OnceValue> once{};
    workers.clear();
    for (int i = 0; i < 8; ++i) workers.emplace_back([&] { CHECK(once.Get()->value == 42); });
    for (auto& thread : workers) thread.join();
    CHECK(OnceValue::constructions.load() == 1);
    once.Get()->~OnceValue();
    Blizzard::Thread::ThreadRecord* record;
    CHECK(Blizzard::Thread::Create(ThreadTLS, nullptr, &record, "core-test", 0, 0));
    CHECK(Blizzard::Thread::WaitForever(record));
    CHECK(!Blizzard::Thread::IsRunning(record));
    CHECK(Blizzard::Thread::Close(record));
    CHECK(destroyed.load() == 9);
    struct Detached { SEvent started{1, 0}; SEvent proceed{1, 0}; } detached;
    auto detachedFunction = [](void* pointer) -> uint32_t {
        auto& state = *static_cast<Detached*>(pointer);
        Local& local = localObject; (void)local;
        state.started.Set();
        CHECK(state.proceed.Wait(5000) == 0);
        return 0;
    };
    CHECK(Blizzard::Thread::Create(detachedFunction, &detached, &record, "detached", 0, 0));
    CHECK(detached.started.Wait(5000) == 0);
    CHECK(Blizzard::Thread::Close(record)); // Drop caller ownership before callback finishes.
    detached.proceed.Set();
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (OsLiveThreadCount() && std::chrono::steady_clock::now() < deadline) OsSleep(1);
    CHECK(!OsLiveThreadCount() && destroyed.load() == 10);
    OsTlsFree(replacement);
    CSRWLock shared;
    SEvent secondReader(1, 0);
    shared.Enter(0);
    std::thread reader([&] { shared.Enter(0); secondReader.Set(); shared.Leave(0); });
    CHECK(secondReader.Wait(2000) == 0); // Shared readers can overlap.
    reader.join();
    std::atomic<bool> writerEntered{false};
    std::thread writer([&] { shared.Enter(1); writerEntered.store(true); shared.Leave(1); });
    OsSleep(5);
    CHECK(!writerEntered.load());
    shared.Leave(0);
    writer.join();
    CHECK(writerEntered.load());
    Blizzard::Lock::EventObject event;
    CHECK(!Blizzard::Lock::EventWait(0, event.m_event));
    Blizzard::Lock::EventSet(event.m_event);
    CHECK(Blizzard::Lock::EventWait(0, event.m_event) == &event.m_event);
    CHECK(!Blizzard::Lock::EventWait(0, event.m_event));
}
void LogsAndTime() {
    HSLOG log;
    CHECK(SLogCreate("core-services-test.log", SLOG_FLAG_OPENFILE, &log));
    std::vector<std::thread> writers;
    for (int i = 0; i < 4; ++i) writers.emplace_back([=] {
        for (int j = 0; j < 50; ++j) SLogWrite(log, "worker=%d line=%d", i, j);
    });
    for (auto& thread : writers) thread.join();
    SLogFlush(log); SLogClose(log);
    CHECK(SLogCreate("core-services-test.log", SLOG_FLAG_OPENFILE | SLOG_FLAG_APPEND, &log));
    SLogWrite(log, "appended"); SLogClose(log);
    std::ifstream input("core-services-test.log");
    std::string line; int lines = 0;
    while (std::getline(input, line)) { CHECK(!line.empty()); ++lines; }
    CHECK(lines == 201);
    input.close(); std::remove("core-services-test.log");
    CHECK(!SLogCreate("", 0, &log));
    for (auto method : {OsTimeManager::SystemMethod1, OsTimeManager::SystemMethod2}) {
        OsTimeStartup(method);
        const auto clocks = OsGetAsyncTimeClocks();
        const auto frequency = OsGetAsyncClocksPerSecond();
        const auto milliseconds = OsGetAsyncTimeMs();
        OsSleep(10);
        CHECK(OsGetAsyncTimeClocks() > clocks);
        CHECK(OsGetAsyncTimeMs() - milliseconds >= 8);
        CHECK(frequency == (method == OsTimeManager::SystemMethod1 ? 1000 : 1000000000));
        OsTimeShutdown();
    }
    for (int64_t value : {int64_t(0), int64_t(-1), int64_t(-86400000000000LL),
                          int64_t(951827696123456789LL), int64_t(-9000000000000000000LL),
                          INT64_MIN + 1, INT64_MAX}) {
        Blizzard::Time::TimeRec rec;
        Blizzard::Time::BreakTime(value, rec);
        CHECK(Blizzard::Time::MakeTime(rec) == value);
    }
    CHECK(Blizzard::Time::FromFileTime(125911584000000000ULL) == 0);
    auto tick = Blizzard::Time::Milliseconds();
    auto utc = Blizzard::Time::GetTimestamp();
    OsSleep(2);
    CHECK(Blizzard::Time::Milliseconds() >= tick && Blizzard::Time::GetTimestamp() > utc);
    char timestamp[32]; OsGetTimeStamp(timestamp, sizeof(timestamp)); CHECK(std::strlen(timestamp) == 13);
    uint8_t random[1024];
    std::memset(random, 0xA5, sizeof(random));
    OsSecureRandom(random, sizeof(random));
    for (size_t offset = 0; offset < sizeof(random); offset += 256) {
        bool changed = false;
        for (size_t i = offset; i < offset + 256; ++i) changed |= random[i] != 0xA5;
        CHECK(changed);
    }
    // This checks delivery, bounds and repeated calls, not RNG quality.
    OsSecureRandom(nullptr, 0);
    struct Guard { uint8_t before = 17; uint8_t data[32]; uint8_t after = 23; } guard;
    OsSecureRandom(guard.data, sizeof(guard.data));
    CHECK(guard.before == 17 && guard.after == 23);
}
int main(int argc, char**) {
    if (argc > 1) { OsSecureRandom(nullptr, 1); return 0; }
    StormRtlInitialize();
    Allocation(); WaitSemantics(); LocksAndTLS(); LogsAndTime();
    CHECK(!OsLiveThreadCount());
    StormRtlDestroy();
    std::puts("Core allocation, waits, locks, TLS, logging, time and randomness checks passed.");
}
