#include "Engine/Source/Storm/H/storm.h"
#include "Engine/Source/Storm/Source/SThread.h"
#include "Engine/Source/Os/Portable/Runtime.h"
#include "Engine/Source/Os/OsSecureRandom.h"
#include "Engine/Source/Os/Time.h"
#include <atomic>
#include <cfenv>
#include <cstdio>
#include <cstdlib>

// Exercise allocator/log availability in both pre-main and late static lifetime.
struct GlobalLifetime {
    void* memory = SMemAlloc(32, __FILE__, __LINE__, SMEM_FLAG_ZEROMEMORY);
    GlobalLifetime() {
        HSLOG log;
        if (!memory || !SLogCreate("native-core-static.log", SLOG_FLAG_NOFILE, &log)) std::abort();
        SLogWrite(log, "global constructor");
        SLogClose(log);
    }
    ~GlobalLifetime() {
        HSLOG log;
        if (!SLogCreate("native-core-static.log", SLOG_FLAG_NOFILE, &log)) std::abort();
        SLogWrite(log, "global destructor");
        SLogClose(log);
        SMemFree(memory, __FILE__, __LINE__, 0);
    }
} globalLifetime;

struct Session { std::atomic<int> result{1}; };
uint32_t SessionMain(void* argument) {
    auto& session = *static_cast<Session*>(argument);
    if (OsCurrentThreadStackSize() < 8 * 1024 * 1024) return 1;
    if (std::fesetenv(FE_DFL_ENV) || std::fesetround(FE_TONEAREST)) return 1;
    StormRtlInitialize();
    OsTimeStartup(OsTimeManager::BestAvailable);
    HSLOG log;
    if (!SLogCreate("native-core.log", SLOG_FLAG_OPENFILE, &log)) return 1;
    uint8_t entropy[32];
    OsSecureRandom(entropy, sizeof(entropy));
    SLogWrite(log, "Native core initialized: stack=%zu, clock=%s, random source available",
              OsCurrentThreadStackSize(), GetTimingMethodName(OsTimeGetTimingMethod()));
    SLogFlush(log);
    SLogClose(log);
    OsTimeShutdown();
    StormRtlDestroy();
    session.result.store(0);
    return 0;
}
int main() {
    // Ordinary CRT startup. No custom pre-CRT entry, SEH emulation or fibers.
    // The process/main thread stays free for the later SDL main-thread lifecycle;
    // this M1 executable exercises only core services on an explicit 8 MiB stack.
    for (int iteration = 0; iteration < 3; ++iteration) {
        Session session;
        SThread worker;
        if (!SThread::Create(SessionMain, &session, worker, nullptr, 8 * 1024 * 1024)) return 1;
        if (worker.Wait(10000) != 0 || session.result.load()) return 1;
    }
    if (OsLiveThreadCount()) return 1;
    std::puts("Native core initialized and shut down cleanly (3 lifecycles).");
    return 0;
}
