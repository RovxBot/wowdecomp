// Owner-thread adaptation of Event/EvtQueue.cpp's priority/marker algorithm.
// SDL owns the outer loop. Original handler IDs, order and propagation remain.
#include "Loop.h"
#include "Engine/Source/FrameScript/Portable/Runtime.h"
#include <array>
#include <cmath>
#include <cstring>
#include <list>
#include <memory>
#include <stdexcept>
#include <thread>
namespace {
struct Handler { EVENTHANDLERFUNC function; void* param; float priority; bool marker = false; };
struct Context {
    std::array<std::list<Handler>, EVENTIDS> handlers;
    std::thread::id owner = std::this_thread::get_id();
    bool closing = false;
    unsigned depth = 0;
};
std::unique_ptr<Context> current;
void Check() {
    if (!current || current->owner != std::this_thread::get_id())
        throw std::logic_error("Native Event requires its owner thread");
}
void Id(EVENTID id) {
    uint32_t value; static_assert(sizeof(value) == sizeof(id));
    std::memcpy(&value, &id, sizeof(value));
    if (value >= EVENTIDS) throw std::invalid_argument("Invalid Event ID");
}
}
namespace EventNative {
void Dispatch(EVENTID id, const void* data) {
    Check(); Id(id);
    auto& list = current->handlers[id];
    auto marker = list.insert(list.begin(), {nullptr, nullptr, 0, true});
    struct Scope {
        std::list<Handler>& list; std::list<Handler>::iterator marker;
        ~Scope() { list.erase(marker); --current->depth; }
    } scope{list, marker};
    ++current->depth;
    while (std::next(marker) != list.end()) {
        auto handler = std::next(marker);
        list.splice(std::next(handler), list, marker);
        if (handler->marker) continue;
        // A callback may unregister itself. Copy before calling it.
        const auto function = handler->function; void* param = handler->param;
        if (!function(data, param)) break;
    }
}
void Tick(float elapsed, uint32_t time) {
    Check();
    if (!std::isfinite(elapsed) || elapsed < 0) throw std::invalid_argument("Invalid Event elapsed time");
    EVENT_DATA_UPDATE update{elapsed, time};
    EVENT_DATA_IDLE idle{elapsed, time};
    Dispatch(EVENT_ID_POLL, nullptr);
    if (!current->closing) Dispatch(EVENT_ID_UPDATE, &update);
    if (!current->closing) Dispatch(EVENT_ID_IDLE, &idle);
    if (!current->closing) Dispatch(EVENT_ID_PAINT, nullptr);
}
bool Closing() { Check(); return current->closing; }
}
void EventInitialize(uint32_t threads, int32_t server) {
    if (current || threads != 1 || server) throw std::logic_error("Native Event supports one client owner");
    current = std::make_unique<Context>();
}
void EventDestroy() {
    Check();
    if (current->depth) throw std::logic_error("Finish Event dispatch before destroying the context");
    EventNative::Dispatch(EVENT_ID_DESTROY, nullptr); current.reset();
}
HEVENTCONTEXT EventGetCurrentContext() { Check(); return reinterpret_cast<HEVENTCONTEXT>(current.get()); }
HEVENTCONTEXT EventCreateContextEx(int32_t interactive, EVENTHANDLERFUNC init, EVENTHANDLERFUNC destroy, uint32_t, uint32_t) {
    Check(); if (!interactive) throw std::logic_error("Only the interactive client context is installed");
    if (destroy) EventRegister(EVENT_ID_DESTROY, destroy);
    if (init) init(nullptr, nullptr);
    return EventGetCurrentContext();
}
void EventRegisterEx(EVENTID id, EVENTHANDLERFUNC function, void* param, float priority) {
    Check(); Id(id);
    if (!function || !std::isfinite(priority)) throw std::invalid_argument("Invalid Event handler");
    auto& list = current->handlers[id]; auto position = list.begin();
    while (position != list.end() && (position->marker || priority < position->priority)) ++position;
    list.insert(position, {function, param, priority});
}
void EventUnregisterEx(EVENTID id, EVENTHANDLERFUNC function, void* param, uint32_t flags) {
    Check(); if (flags & 1) Id(id);
    for (size_t i = 0; i < EVENTIDS; ++i) {
        if ((flags & 1) && i != size_t(id)) continue;
        current->handlers[i].remove_if([&](const Handler& h) {
            return !h.marker && (!(flags & 2) || h.function == function) && (!(flags & 4) || h.param == param);
        });
    }
}
void EventRegister(EVENTID id, EVENTHANDLERFUNC handler) { EventRegisterEx(id, handler, nullptr, EVENT_PRIORITY_NORMAL); }
void EventUnregister(EVENTID id, EVENTHANDLERFUNC handler) { EventUnregisterEx(id, handler, nullptr, 3); }
void EventPostClose() { Check(); current->closing = true; }
void EventInitiateShutdown() { EventPostClose(); }
