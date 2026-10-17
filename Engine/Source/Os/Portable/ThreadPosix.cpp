#include "Engine/Source/Os/Portable/Runtime.h"
#include <pthread.h>
#include <cstdlib>
#include <memory>
#include <unistd.h>

namespace {
struct Launch { OsThreadEntry entry; void* argument; };
void* Start(void* argument) {
    std::unique_ptr<Launch> launch(static_cast<Launch*>(argument));
    launch->entry(launch->argument);
    return nullptr;
}
}
bool OsLaunchThread(OsThreadEntry entry, void* argument, size_t stackSize) {
    pthread_attr_t attributes;
    if (pthread_attr_init(&attributes)) return false;
    const size_t page = static_cast<size_t>(sysconf(_SC_PAGESIZE));
    const size_t minimum = static_cast<size_t>(PTHREAD_STACK_MIN);
    stackSize = stackSize < minimum ? minimum : stackSize;
    if (stackSize > SIZE_MAX - page) { pthread_attr_destroy(&attributes); return false; }
    stackSize = (stackSize + page - 1) / page * page;
    int result = pthread_attr_setstacksize(&attributes, stackSize);
    if (!result) result = pthread_attr_setdetachstate(&attributes, PTHREAD_CREATE_DETACHED);
    auto launch = std::make_unique<Launch>(Launch{entry, argument});
    pthread_t thread;
    if (!result) result = pthread_create(&thread, &attributes, Start, launch.get());
    pthread_attr_destroy(&attributes);
    if (result) return false;
    launch.release();
    return true;
}
size_t OsCurrentThreadStackSize() {
#if defined(__APPLE__)
    return pthread_get_stacksize_np(pthread_self());
#else
    pthread_attr_t attributes;
    if (pthread_getattr_np(pthread_self(), &attributes)) return 0;
    void* base;
    size_t size = 0;
    pthread_attr_getstack(&attributes, &base, &size);
    pthread_attr_destroy(&attributes);
    return size;
#endif
}
