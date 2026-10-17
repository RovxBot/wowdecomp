#ifndef ENGINE_SOURCE_STORM_PORTABLE_STHREAD_H
#define ENGINE_SOURCE_STORM_PORTABLE_STHREAD_H
#include "Engine/Source/Storm/H/SCritSect.h"
#include "Engine/Source/Storm/Source/SLock.h"
class SThread : public SSyncObject {
public:
    static int32_t Create(uint32_t (STORMAPI* function)(void*), void* parameter, SThread& thread, char* name, uint32_t stackSize);
    // Priority changes require a platform policy; report unavailable explicitly.
    int32_t SetPriority(int32_t) { return 0; }
};
class CInitCritSect {
    CCritSect m_mutex;
    bool m_initialized = false;
public:
    int32_t Enter() { m_mutex.Enter(); bool first = !m_initialized; m_initialized = true; return first; }
    void Leave() { m_mutex.Leave(); }
};
#endif
