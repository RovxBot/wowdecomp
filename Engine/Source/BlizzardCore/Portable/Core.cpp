#include "Engine/Source/BlizzardCore/Source/System/Lock/Lock.h"
#include "Engine/Source/BlizzardCore/Source/System/Memory/MemoryStorm.h"
#include "Engine/Source/BlizzardCore/Source/System/Thread/ThreadLocalStorage.h"
#include "Engine/Source/Os/TimeManager.h"
#include "Engine/Source/Storm/H/storm.h"
#include <mutex>

void* Blizzard::Memory::Allocate(uint32_t bytes) { return SMemAlloc(bytes, __FILE__, __LINE__, 0); }
void* Blizzard::Memory::Allocate(uint32_t bytes, uint32_t flags, const char* file, int32_t line) {
    return SMemAlloc(bytes, file, line, flags & 1 ? SMEM_FLAG_ZEROMEMORY : 0);
}
void Blizzard::Memory::Free(void* pointer) { SMemFree(pointer, __FILE__, __LINE__, 0); }
void Blizzard::Memory::Free(void* pointer, uint32_t flags, const char* file, int32_t line) {
    SMemFree(pointer, file, line, flags);
}
void* operator new(size_t bytes, uint32_t flags, const char* file, int32_t line) {
    return SMemAlloc(bytes, file, line, flags & 1 ? SMEM_FLAG_ZEROMEMORY : 0);
}
void operator delete(void* pointer, uint32_t flags, const char* file, int32_t line) {
    SMemFree(pointer, file, line, flags);
}
int32_t Blizzard::Lock::Atomic::Increment() { return ++m_value; }
int32_t Blizzard::Lock::Atomic::Decrement() { return --m_value; }
void Blizzard::Lock::MutexCreate(Mutex& mutex) { mutex.implementation = new std::recursive_mutex; }
void Blizzard::Lock::MutexDestroy(Mutex& mutex) { delete mutex.implementation; mutex.implementation = nullptr; }
void Blizzard::Lock::MutexEnter(Mutex& mutex) { mutex.implementation->lock(); }
void Blizzard::Lock::MutexLeave(Mutex& mutex) { mutex.implementation->unlock(); }
Blizzard::Lock::RMutex::RMutex() { MutexCreate(m_mutex); }
Blizzard::Lock::RMutex::~RMutex() { MutexDestroy(m_mutex); }
Blizzard::Lock::ScopedMutex::ScopedMutex(Mutex& mutex) : m_mutex(&mutex) { MutexEnter(mutex); }
Blizzard::Lock::ScopedMutex::~ScopedMutex() { MutexLeave(*m_mutex); }
void Blizzard::Lock::EventCreate(Event& event) { event = OsCreateEvent(false, false); }
void Blizzard::Lock::EventDestroy(Event& event) { OsCloseSync(event); event = nullptr; }
void Blizzard::Lock::EventSet(Event& event) { OsSetEvent(event, true); }
void Blizzard::Lock::EventWaitForever(Event& event) {
    if (OsWaitObjects(1, &event, false, OS_WAIT_INFINITE) != 0)
        SErrDisplayAppFatalMessage("Invalid BlizzardCore event wait", __FILE__, __LINE__);
}
Blizzard::Lock::Event* Blizzard::Lock::EventWait(uint32_t timeout, Event& event) {
    return OsWaitObjects(1, &event, false, timeout) == 0 ? &event : nullptr;
}
Blizzard::Lock::EventObject::EventObject() { EventCreate(m_event); }
Blizzard::Lock::EventObject::~EventObject() { EventDestroy(m_event); }
void Blizzard::Process::Sleep(uint32_t milliseconds) { OsSleep(milliseconds); }

namespace {
std::mutex& SlotMutex() { static std::mutex mutex; return mutex; }
}
bool System_Thread::AllocateTLSSlot(Blizzard::Thread::TLSSlot* slot, Blizzard::Thread::TLSDestructor destructor) {
    std::lock_guard<std::mutex> lock(SlotMutex());
    if (slot->m_key) return true;
    uint32_t key = OsAllocateTLS(destructor);
    if (key == UINT32_MAX) return false;
    slot->m_destructor = destructor;
    slot->m_key = ~key;
    return true;
}
bool System_Thread::AllocateLocalStorage(Blizzard::Thread::TLSSlot* slot, Blizzard::Thread::TLSDestructor destructor) {
    return AllocateTLSSlot(slot, destructor);
}
void System_Thread::InternalSetLocalStorage(const Blizzard::Thread::TLSSlot* slot, const void* value) {
    if (!OsSetTLS(~slot->m_key, const_cast<void*>(value)))
        SErrDisplayAppFatalMessage("Invalid BlizzardCore TLS slot", __FILE__, __LINE__);
}
void* Blizzard::Thread::RegisterLocalStorage(TLSSlot* slot, void* (*constructor)(void*), void* parameter, TLSDestructor destructor) {
    // Slot publication is protected even when a zero-initialized LocalObject is
    // first touched concurrently; user constructors run outside the registry lock.
    if (!System_Thread::AllocateTLSSlot(slot, destructor)) return nullptr;
    if (void* value = OsGetTLS(~slot->m_key)) return value;
    void* value = constructor(parameter);
    System_Thread::InternalSetLocalStorage(slot, value);
    return value;
}
void System_Thread::RunTLSDestructors() { OsRunTLSDestructors(); }
