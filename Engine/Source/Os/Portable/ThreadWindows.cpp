#include "Engine/Source/Os/Portable/Runtime.h"
#include <windows.h>
#include <process.h>
#include <memory>

namespace {
struct Launch { OsThreadEntry entry; void* argument; };
unsigned __stdcall Start(void* argument) {
    std::unique_ptr<Launch> launch(static_cast<Launch*>(argument));
    launch->entry(launch->argument);
    return 0;
}
}
bool OsLaunchThread(OsThreadEntry entry, void* argument, size_t stackSize) {
    if (stackSize > UINT32_MAX) return false;
    auto launch = std::make_unique<Launch>(Launch{entry, argument});
    auto handle = reinterpret_cast<HANDLE>(_beginthreadex(nullptr, static_cast<unsigned>(stackSize),
                                                         Start, launch.get(), 0, nullptr));
    if (!handle) return false;
    launch.release();
    CloseHandle(handle);
    return true;
}
size_t OsCurrentThreadStackSize() {
    ULONG_PTR low, high;
    GetCurrentThreadStackLimits(&low, &high);
    return high - low;
}
