#include "AsyncFile.h"
#include "Engine/Source/BlizzardCore/Portable/Thread.h"
#include "Engine/Source/Os/Portable/Runtime.h"
#include "Engine/Source/Storm/H/storm.h"
#include <algorithm>
#include <cfenv>
#include <condition_variable>
#include <cstdio>
#include <deque>
#include <mutex>
#include <thread>

namespace Services { namespace Native {
namespace {
enum class Phase { Queued, Reading, Ready, Delivering, Complete };
}
struct FileRequest {
    FileQueue::Operation operation;
    FileQueue::Callback callback;
    Cancellation cancellation;
    ReadResult result;
    uint8_t priority;
    std::atomic<Phase> phase{Phase::Queued};
    const void* owner;
};
struct FileQueue::State {
    std::mutex mutex;
    std::condition_variable changed;
    std::deque<Ticket> queued, ready;
    Ticket active;
    Blizzard::Thread::ThreadRecord* worker = nullptr;
    std::thread::id owner = std::this_thread::get_id();
    size_t pending = 0;
    bool stopping = false, pumping = false, workerReady = false, workerFailed = false;
};
void FileQueue::CheckOwner() const {
    if (std::this_thread::get_id() != state->owner) throw std::logic_error("Async file callbacks require their owner thread");
}
FileQueue::FileQueue() : state(new State) {
    if (!Blizzard::Thread::Create(Worker, state.get(), &state->worker, "NativeFileRead", 0, 8 * 1024 * 1024))
        throw std::runtime_error("Cannot create native file worker");
    std::unique_lock<std::mutex> lock(state->mutex);
    state->changed.wait(lock, [&] { return state->workerReady; });
    if (state->workerFailed) {
        lock.unlock();
        Blizzard::Thread::WaitForever(state->worker); Blizzard::Thread::Close(state->worker);
        state->worker = nullptr;
        throw std::runtime_error("Native file worker stack/floating-point initialization failed");
    }
}
FileQueue::~FileQueue() {
    if (std::this_thread::get_id() != state->owner || state->pumping) std::terminate();
    // Callback errors never strand worker/state lifetime during unwinding.
    try { Shutdown(); }
    catch (...) { std::fputs("Native file shutdown callback failed\n", stderr); }
}
uint32_t FileQueue::Worker(void* context) {
    auto& state = *static_cast<State*>(context);
    {
        std::lock_guard<std::mutex> lock(state.mutex);
        state.workerFailed = OsCurrentThreadStackSize() < 8 * 1024 * 1024 ||
                             std::fesetenv(FE_DFL_ENV) || std::fesetround(FE_TONEAREST);
        state.workerReady = true;
        state.changed.notify_all();
        if (state.workerFailed) return 1;
    }
    for (;;) {
        Ticket request;
        {
            std::unique_lock<std::mutex> lock(state.mutex);
            state.changed.wait(lock, [&] { return state.stopping || !state.queued.empty(); });
            if (state.queued.empty() && state.stopping) break;
            request = state.queued.front(); state.queued.pop_front();
            request->phase = Phase::Reading; state.active = request;
        }
        try {
            if (!request->cancellation.Requested()) request->result = request->operation(request->cancellation);
        } catch (const std::exception& error) {
            request->result.status = ReadStatus::Failed; request->result.error = error.what();
        } catch (...) {
            request->result.status = ReadStatus::Failed; request->result.error = "Unknown native file read failure";
        }
        {
            std::lock_guard<std::mutex> lock(state.mutex);
            if (request->cancellation.Requested()) {
                request->result = {}; request->result.status = ReadStatus::Cancelled;
            }
            request->operation = {}; // release file/package ownership after read
            request->phase = Phase::Ready;
            state.active.reset(); state.ready.push_back(request); state.changed.notify_all();
        }
    }
    return 0;
}
Ticket FileQueue::Submit(Operation operation, Callback callback, uint8_t priority, bool urgent) {
    if (!operation || !callback) throw std::invalid_argument("Async read requires operation and completion callback");
    auto request = std::make_shared<FileRequest>();
    request->operation = std::move(operation); request->callback = std::move(callback); request->priority = priority;
    request->owner = state.get();
    std::lock_guard<std::mutex> lock(state->mutex);
    if (state->stopping) throw std::logic_error("Native file queue is shut down");
    auto position = std::find_if(state->queued.begin(), state->queued.end(), [&](const Ticket& other) {
        return priority < other->priority || (urgent && priority == other->priority);
    });
    state->queued.insert(position, request); ++state->pending;
    state->changed.notify_all(); return request;
}
Ticket FileQueue::ReadPackage(std::shared_ptr<const Blizzard::Mopaq::Native::Packages> packages,
                             std::string member, Callback callback, uint8_t priority) {
    if (!packages) throw std::invalid_argument("Missing async package owner");
    return Submit([packages = std::move(packages), member = std::move(member)](const Cancellation&) {
        ReadResult result; result.file = packages->Read(member); return result;
    }, std::move(callback), priority);
}
Ticket FileQueue::ReadHost(std::string path, uint64_t offset, uint32_t bytes, Callback callback, uint8_t priority) {
    if (offset > INT64_MAX || bytes > 256 * 1024 * 1024 || bytes > uint64_t(INT64_MAX) - offset)
        throw std::invalid_argument("Invalid async host read extent");
    return Submit([path = std::move(path), offset, bytes](const Cancellation& cancellation) {
        namespace File = Blizzard::File;
        ReadResult result;
        struct Owner { File::StreamRecord* file = nullptr; ~Owner() { if (file) File::Close(file); } } stream;
        if (!File::Native::Open(path.c_str(), 1 | 4, File::Native::Disposition::OpenExisting, stream.file)) {
            result.status = ReadStatus::Failed; result.systemError = SErrGetLastError(); result.error = "Cannot open async host file"; return result;
        }
        result.file.bytes.resize(bytes);
        size_t completed = 0;
        while (completed < bytes && !cancellation.Requested()) {
            int32_t count = static_cast<int32_t>(std::min<size_t>(bytes - completed, 1024 * 1024));
            if (!File::Read(stream.file, result.file.bytes.data() + completed, static_cast<int64_t>(offset + completed), &count)) {
                result.status = ReadStatus::Failed; result.systemError = SErrGetLastError(); result.error = "Async host read failed"; break;
            }
            if (!count) break;
            completed += count;
        }
        result.file.bytes.resize(completed); return result;
    }, std::move(callback), priority);
}
bool FileQueue::Cancel(const Ticket& ticket) {
    if (!ticket || ticket->owner != state.get()) return false;
    std::lock_guard<std::mutex> lock(state->mutex);
    auto phase = ticket->phase.load();
    if (phase == Phase::Delivering || phase == Phase::Complete) return false;
    ticket->cancellation.requested = true;
    if (phase == Phase::Queued) {
        auto position = std::find(state->queued.begin(), state->queued.end(), ticket);
        state->queued.erase(position);
        ticket->operation = {}; ticket->result.status = ReadStatus::Cancelled;
        ticket->phase = Phase::Ready; state->ready.push_back(ticket);
    }
    state->changed.notify_all(); return true;
}
bool FileQueue::Promote(const Ticket& ticket, uint8_t priority) {
    if (!ticket || ticket->owner != state.get()) return false;
    std::lock_guard<std::mutex> lock(state->mutex);
    if (ticket->phase != Phase::Queued || priority > ticket->priority) return false;
    auto position = std::find(state->queued.begin(), state->queued.end(), ticket);
    state->queued.erase(position); ticket->priority = priority;
    position = std::find_if(state->queued.begin(), state->queued.end(), [&](const Ticket& other) { return priority <= other->priority; });
    state->queued.insert(position, ticket); return true;
}
size_t FileQueue::Poll() {
    CheckOwner();
    if (state->pumping) throw std::logic_error("Recursive native file callback pump");
    state->pumping = true;
    struct Reset { bool& value; ~Reset() { value = false; } } reset{state->pumping};
    size_t count = 0;
    std::exception_ptr exception;
    for (;;) {
        Ticket ticket;
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            if (state->ready.empty()) break;
            ticket = state->ready.front(); state->ready.pop_front();
            if (ticket->cancellation.Requested()) { ticket->result = {}; ticket->result.status = ReadStatus::Cancelled; }
            ticket->phase = Phase::Delivering;
        }
        try { ticket->callback(ticket->result); } catch (...) { if (!exception) exception = std::current_exception(); }
        ticket->callback = {};
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            ticket->phase = Phase::Complete; --state->pending; state->changed.notify_all();
        }
        ++count;
    }
    if (exception) std::rethrow_exception(exception);
    return count;
}
bool FileQueue::Complete(const Ticket& ticket) { return ticket && ticket->phase == Phase::Complete; }
size_t FileQueue::Pending() const { std::lock_guard<std::mutex> lock(state->mutex); return state->pending; }
bool FileQueue::Wait(const Ticket& ticket, std::chrono::milliseconds timeout) {
    CheckOwner();
    if (!ticket || ticket->owner != state.get() || state->pumping) throw std::logic_error("Invalid/reentrant native file wait");
    auto deadline = std::chrono::steady_clock::now() + timeout;
    for (;;) {
        Poll(); if (Complete(ticket)) return true;
        std::unique_lock<std::mutex> lock(state->mutex);
        if (state->changed.wait_until(lock, deadline, [&] { return !state->ready.empty(); })) continue;
        return false;
    }
}
bool FileQueue::WaitAll(std::chrono::milliseconds timeout) {
    CheckOwner();
    if (state->pumping) throw std::logic_error("Reentrant native file wait-all");
    auto deadline = std::chrono::steady_clock::now() + timeout;
    for (;;) {
        Poll(); if (!Pending()) return true;
        std::unique_lock<std::mutex> lock(state->mutex);
        if (state->changed.wait_until(lock, deadline, [&] { return !state->ready.empty(); })) continue;
        return false;
    }
}
void FileQueue::Shutdown() {
    CheckOwner();
    if (state->pumping) throw std::logic_error("Cannot destroy native file queue inside its callback");
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        state->stopping = true;
        if (state->active) state->active->cancellation.requested = true;
        for (auto& ticket : state->ready) ticket->cancellation.requested = true;
        for (auto& ticket : state->queued) {
            ticket->cancellation.requested = true; ticket->operation = {};
            ticket->result.status = ReadStatus::Cancelled; ticket->phase = Phase::Ready; state->ready.push_back(ticket);
        }
        state->queued.clear(); state->changed.notify_all();
    }
    if (state->worker) {
        Blizzard::Thread::WaitForever(state->worker); Blizzard::Thread::Close(state->worker); state->worker = nullptr;
    }
    Poll();
}
}}
