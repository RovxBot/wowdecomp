#ifndef ENGINE_SOURCE_BLIZZARDCORE_PORTABLE_THREAD_H
#define ENGINE_SOURCE_BLIZZARDCORE_PORTABLE_THREAD_H
#include "Engine/Source/BlizzardCore/Source/System/Lock/Lock.h"
#include "Engine/Source/BlizzardCore/Source/System/Thread/ThreadLocalStorage.h"
namespace Blizzard { namespace Thread {
struct ThreadRecord {
    OsSyncHandle* m_handle = nullptr;
    uint32_t m_threadId = 0;
    void* m_param = nullptr;
    uint32_t (*m_function)(void*) = nullptr;
    std::atomic<int32_t> m_state{0};
    Lock::Atomic m_refCount;
    uint32_t m_result = 0;
};
bool Close(ThreadRecord* thread);
bool Create(uint32_t (*function)(void*), void* parameter, ThreadRecord** thread,
            const char* name, int32_t flags, int32_t stackSize);
bool CreateDetached(uint32_t (*function)(void*), void* parameter, const char* name);
bool IsRunning(ThreadRecord* thread);
bool Sleep(uint32_t milliseconds);
bool WaitForever(ThreadRecord* thread);
} }
#endif
