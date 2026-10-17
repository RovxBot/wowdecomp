#ifndef WOWDECOMP_NATIVE_ASYNC_FILE_H
#define WOWDECOMP_NATIVE_ASYNC_FILE_H
#include "Engine/Source/BlizzardCore/Portable/Package.h"
#include <atomic>
#include <chrono>
#include <functional>
#include <memory>

namespace Services { namespace Native {
enum class ReadStatus { Success, Failed, Cancelled };
struct ReadResult {
    ReadStatus status = ReadStatus::Success;
    Blizzard::Mopaq::Native::Loaded file;
    uint32_t systemError = 0;
    std::string error;
};
class Cancellation {
public:
    bool Requested() const { return requested.load(); }
private:
    friend class FileQueue;
    std::atomic<bool> requested{false};
};
struct FileRequest;
using Ticket = std::shared_ptr<FileRequest>;
// Native owner thread API at the Services boundary. Deferred callbacks run only
// in Poll/Wait/Shutdown on the constructor thread, never under the queue lock.
// Keep queue destruction on that thread. No legacy CAsyncObject ABI is exposed.
class FileQueue {
public:
    using Operation = std::function<ReadResult(const Cancellation&)>;
    using Callback = std::function<void(const ReadResult&)>;
    FileQueue();
    ~FileQueue();
    FileQueue(const FileQueue&) = delete;
    FileQueue& operator=(const FileQueue&) = delete;
    Ticket Submit(Operation operation, Callback callback, uint8_t priority = 126, bool urgent = false);
    Ticket ReadPackage(std::shared_ptr<const Blizzard::Mopaq::Native::Packages> packages,
                       std::string member, Callback callback, uint8_t priority = 126);
    // Host reads retain their own open stream and stop at EOF with the actual
    // byte count. They never serve as a fallback for a missing MPQ member.
    Ticket ReadHost(std::string path, uint64_t offset, uint32_t bytes, Callback callback, uint8_t priority = 126);
    // True means cancellation won before callback delivery began. Running
    // regular-file I/O is cooperative: its owners live until worker completion.
    bool Cancel(const Ticket& ticket);
    bool Promote(const Ticket& ticket, uint8_t priority);
    size_t Poll();
    bool Wait(const Ticket& ticket, std::chrono::milliseconds timeout);
    bool WaitAll(std::chrono::milliseconds timeout);
    // Stop accepting requests, cancel pending reads, join worker, deliver every
    // remaining callback once. Calling from inside a callback is rejected.
    void Shutdown();
    size_t Pending() const;
    static bool Complete(const Ticket& ticket);
private:
    struct State;
    std::unique_ptr<State> state;
    static uint32_t Worker(void* context);
    void CheckOwner() const;
};
}}
#endif
