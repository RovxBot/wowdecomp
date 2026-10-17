#include "Engine/Source/BlizzardCore/Source/System/Thread/Thread.h"
#include "Engine/Source/Os/TimeManager.h"

namespace {
uint32_t Start(void* parameter) {
    auto thread = static_cast<Blizzard::Thread::ThreadRecord*>(parameter);
    thread->m_result = thread->m_function(thread->m_param);
    OsRunTLSDestructors();
    thread->m_state.store(-1);
    Blizzard::Thread::Close(thread);
    return 0;
}
}
bool Blizzard::Thread::Create(uint32_t (*function)(void*), void* parameter, ThreadRecord** output,
                              const char* name, int32_t flags, int32_t stackSize) {
    if (!output) return false;
    *output = nullptr;
    if (!function || flags || stackSize < 0) return false;
    auto thread = new ThreadRecord;
    thread->m_function = function;
    thread->m_param = parameter;
    thread->m_state.store(1);
    thread->m_refCount.m_value.store(2);
    thread->m_handle = OsCreateThread(Start, thread, stackSize, &thread->m_threadId, name);
    if (!thread->m_handle) { delete thread; return false; }
    *output = thread;
    return true;
}
bool Blizzard::Thread::Close(ThreadRecord* thread) {
    if (!thread) return false;
    if (!thread->m_refCount.Decrement()) { OsCloseSync(thread->m_handle); delete thread; }
    return true;
}
bool Blizzard::Thread::CreateDetached(uint32_t (*function)(void*), void* parameter, const char* name) {
    ThreadRecord* thread;
    if (!Create(function, parameter, &thread, name, 0, 0)) return false;
    return Close(thread);
}
bool Blizzard::Thread::IsRunning(ThreadRecord* thread) { return thread && thread->m_state.load() >= 0; }
bool Blizzard::Thread::Sleep(uint32_t milliseconds) { OsSleep(milliseconds); return true; }
bool Blizzard::Thread::WaitForever(ThreadRecord* thread) {
    return thread && OsWaitObjects(1, &thread->m_handle, false, OS_WAIT_INFINITE) == 0;
}
