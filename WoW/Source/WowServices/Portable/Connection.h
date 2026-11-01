#ifndef WOWDECOMP_NATIVE_CONNECTION_STATE_H
#define WOWDECOMP_NATIVE_CONNECTION_STATE_H
#include "WoW/Source/WowServices/WowConnection.h"
#include <atomic>
#include <chrono>
#include <memory>
#include <thread>
#include <string>

struct WowNativeLookup {
    std::string host;
    uint32_t address = 0;
    bool success = false;
    std::atomic<bool> done{false};
};
struct WowConnection::Native {
    std::thread::id owner = std::this_thread::get_id();
    WowConnectionNet* network = nullptr;
    std::shared_ptr<WowNativeLookup> lookup;
    std::chrono::steady_clock::time_point deadline;
    uint64_t generation = 0;
    bool timed = false;
    bool pendingResponse = false;
    bool ready = false;
    bool failed = false;
};
#endif
