#ifndef ENGINE_SOURCE_STORM_PORTABLE_SCRITSECT_H
#define ENGINE_SOURCE_STORM_PORTABLE_SCRITSECT_H
#include <cstdint>
#include <mutex>
class CCritSect {
    std::recursive_mutex m_mutex;
public:
    void Enter() { m_mutex.lock(); }
    void Enter(int32_t) { Enter(); }
    void Leave() { m_mutex.unlock(); }
    void Leave(int32_t) { Leave(); }
};
#endif
