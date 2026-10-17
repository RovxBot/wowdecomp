#ifndef ENGINE_SOURCE_STORM_PORTABLE_SLOCK_H
#define ENGINE_SOURCE_STORM_PORTABLE_SLOCK_H
#include "Engine/Source/Storm/H/storm.h"
#include "Engine/Source/Os/Portable/Runtime.h"
#include <mutex>
#include <shared_mutex>
class SCritSect {
    std::recursive_mutex m_mutex;
public:
    void Enter() { m_mutex.lock(); }
    void Leave() { m_mutex.unlock(); }
};
class SSyncObject {
public:
    OsSyncHandle* m_opaqueData = nullptr;
    SSyncObject() = default;
    SSyncObject(const SSyncObject&) = delete;
    SSyncObject& operator=(const SSyncObject&) = delete;
    ~SSyncObject() { Close(); }
    static uint32_t STORMAPI WaitMultiplePtr(uint32_t count, SSyncObject** objects, int32_t waitAll, uint32_t timeout);
    void Close() { OsCloseSync(m_opaqueData); m_opaqueData = nullptr; }
    int32_t Valid() { return m_opaqueData != nullptr; }
    uint32_t Wait(uint32_t timeout) { return OsWaitObjects(1, &m_opaqueData, false, timeout); }
};
class SEvent : public SSyncObject {
public:
    SEvent(int32_t manualReset, int32_t initialState) { m_opaqueData = OsCreateEvent(manualReset, initialState); }
    int32_t Reset() { return OsSetEvent(m_opaqueData, false); }
    int32_t Set() { return OsSetEvent(m_opaqueData, true); }
};
class SMutex : public SSyncObject {
public:
    SMutex() { Create(0, nullptr); }
    void Create(int32_t initialOwner, const char* name) { Close(); m_opaqueData = OsCreateMutex(initialOwner, name); }
    int32_t Release() { return OsReleaseMutex(m_opaqueData); }
};
class SSemaphore : public SSyncObject {
public:
    SSemaphore(int32_t initialCount, int32_t maximumCount) {
        if (initialCount >= 0 && maximumCount > 0)
            m_opaqueData = OsCreateSemaphore(initialCount, maximumCount);
    }
    int32_t Signal(uint32_t count) { return OsReleaseSemaphore(m_opaqueData, count); }
};
class CSRWLock {
    std::shared_mutex m_mutex;
public:
    void Enter(int32_t writing) { if (writing) m_mutex.lock(); else m_mutex.lock_shared(); }
    void Leave(int32_t writing) { if (writing) m_mutex.unlock(); else m_mutex.unlock_shared(); }
};
#endif
