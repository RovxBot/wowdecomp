#ifndef ENGINE_SOURCE_OS_PORTABLE_RUNTIME_H
#define ENGINE_SOURCE_OS_PORTABLE_RUNTIME_H

#include <cstddef>
#include <cstdint>

// Native runtime objects have their host ABI. They are never serialized or
// shared with a Windows client object compiled against the legacy runtime.
struct OsSyncHandle;
constexpr uint32_t OS_WAIT_TIMEOUT = 258;
constexpr uint32_t OS_WAIT_FAILED = 0xFFFFFFFFu;
constexpr uint32_t OS_WAIT_ABANDONED = 128;
constexpr uint32_t OS_WAIT_INFINITE = 0xFFFFFFFFu;

OsSyncHandle* OsCreateEvent(bool manualReset, bool initialState);
OsSyncHandle* OsCreateMutex(bool initialOwner, const char* name);
OsSyncHandle* OsCreateSemaphore(uint32_t initialCount, uint32_t maximumCount);
void OsCloseSync(OsSyncHandle* handle);
bool OsSetEvent(OsSyncHandle* handle, bool signaled);
bool OsReleaseMutex(OsSyncHandle* handle);
bool OsReleaseSemaphore(OsSyncHandle* handle, uint32_t count);
uint32_t OsWaitObjects(uint32_t count, OsSyncHandle* const* handles, bool waitAll, uint32_t timeout);

using OsThreadProc = uint32_t (*)(void*);
OsSyncHandle* OsCreateThread(OsThreadProc function, void* argument, uint32_t stackSize,
                             uint32_t* threadId, const char* name);
uint32_t OsCurrentThreadId();
void OsEnsureThreadExit();
void OsAtThreadExit(void (*callback)(void*), void* argument);
size_t OsCurrentThreadStackSize();
// Used only by the platform thread launcher. Completion is published by a
// thread-local exit guard after registered runtime TLS destructors have run.
using OsThreadEntry = void (*)(void*);
bool OsLaunchThread(OsThreadEntry entry, void* argument, size_t stackSize);

using OsTLSDestructor = void (*)(void*);
uint32_t OsAllocateTLS(OsTLSDestructor destructor);
bool OsFreeTLS(uint32_t key);
void* OsGetTLS(uint32_t key);
bool OsSetTLS(uint32_t key, void* value);
void OsRunTLSDestructors();
size_t OsLiveThreadCount();

#endif
