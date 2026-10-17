#include "Engine/Source/Storm/Source/SThread.h"
#include <array>

uint32_t STORMAPI SSyncObject::WaitMultiplePtr(uint32_t count, SSyncObject** objects, int32_t waitAll, uint32_t timeout) {
    if (!objects || count > 64) return OS_WAIT_FAILED;
    std::array<OsSyncHandle*, 64> handles{};
    uint32_t used = 0;
    // Preserve Storm's compacted index convention for null/invalid entries.
    for (uint32_t i = 0; i < count; ++i)
        if (objects[i] && objects[i]->m_opaqueData) handles[used++] = objects[i]->m_opaqueData;
    return OsWaitObjects(used, handles.data(), waitAll, timeout);
}
int32_t SThread::Create(uint32_t (STORMAPI* function)(void*), void* parameter, SThread& thread, char* name, uint32_t stackSize) {
    if (thread.Valid()) return 0;
    thread.m_opaqueData = OsCreateThread(function, parameter, stackSize, nullptr, name);
    return thread.Valid();
}
void* SCreateThread(uint32_t stackSize, uint32_t (STORMAPI* function)(void*), void* argument,
                    uint32_t flags, uint32_t* id, char* name) {
    // Suspended creation is not emulated by running a callback early.
    if (flags) { SErrSetLastError(STORM_ERROR_INVALID_PARAMETER); return nullptr; }
    return OsCreateThread(function, argument, stackSize, id, name);
}
void* STORMAPI SCreateThread(uint32_t (STORMAPI* function)(void*), void* argument, uint32_t* id,
                             void* linuxData, char* name, uint32_t stackSize) {
    if (linuxData) { SErrSetLastError(STORM_ERROR_INVALID_PARAMETER); return nullptr; }
    return SCreateThread(stackSize, function, argument, 0, id, name);
}
uint32_t STORMAPI SGetCurrentThreadId() { return OsCurrentThreadId(); }
