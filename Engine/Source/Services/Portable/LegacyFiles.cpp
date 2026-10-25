#include "LegacyFiles.h"
#include "Engine/Source/Services/AsyncFile.h"
#include "Engine/Source/BlizzardCore/SFile2-Core.h"
#include "Engine/Source/Os/System.h"
#include "Engine/Source/Storm/H/storm.h"
#include <algorithm>
#include <cstring>
#include <map>
#include <thread>
#include <vector>

namespace Services { namespace Native {
namespace {
LegacyFiles::State* active = nullptr;
constexpr size_t MaxFile = 256 * 1024 * 1024;
using Bytes = Blizzard::Mopaq::Native::Bytes;
struct OpenFile {
    std::unique_ptr<SFileImpl> handle;
    Bytes bytes;
    std::string name;
    CAsyncObject* asyncOwner = nullptr;
};
struct Request {
    std::unique_ptr<CAsyncObject> object{new CAsyncObject{}};
    std::shared_ptr<OpenFile> file;
    Ticket ticket;
    ReadResult result;
    void* destination = nullptr;
    uint32_t size = 0;
    bool ready = false, retired = false, submitted = false, delivered = false;
};
}
struct LegacyFiles::State {
    FileQueue& queue;
    Reader reader;
    std::thread::id owner = std::this_thread::get_id();
    std::map<SFileImpl*, std::shared_ptr<OpenFile>> files;
    std::map<CAsyncObject*, std::shared_ptr<Request>> requests;
    std::vector<std::shared_ptr<Request>> ready;
    std::string error;
    bool delivering = false;
    State(FileQueue& q, Reader r) : queue(q), reader(std::move(r)) {}
    void Check() const {
        if (std::this_thread::get_id() != owner) throw std::logic_error("Legacy files require their owner thread");
    }
    int32_t Error(const char* message, uint32_t code = 87) {
        error = message; SErrSetLastError(code); return 0;
    }
    std::shared_ptr<OpenFile> File(SFileImpl* file) {
        Check();
        auto i = files.find(file);
        if (i == files.end()) throw std::invalid_argument("Invalid native SFile handle");
        return i->second;
    }
    std::shared_ptr<Request> Object(CAsyncObject* object) {
        Check();
        auto i = requests.find(object);
        if (i == requests.end()) throw std::invalid_argument("Invalid native CAsyncObject");
        return i->second;
    }
    void Retire(std::shared_ptr<Request> request) {
        auto* object = request->object.get();
        std::shared_ptr<OpenFile> file;
        if (object->m_file) {
            file = File(object->m_file);
            if (file->asyncOwner && file->asyncOwner != object)
                throw std::logic_error("Another native async object owns this file");
        }
        if (request->submitted && file != request->file)
            throw std::logic_error("Native async file changed while pending");
        request->retired = true;
        if (request->ticket) queue.Cancel(request->ticket);
        if (file) {
            file->asyncOwner = nullptr;
            files.erase(object->m_file);
        }
        requests.erase(object);
        request->object.reset();
    }
    void Deliver(bool unload = false) {
        Check();
        if (delivering) throw std::logic_error("Recursive legacy file callback delivery");
        delivering = true;
        struct Reset { bool& flag; ~Reset() { flag = false; } } reset{delivering};
        // Detach before callbacks: opening the next skin/animation can pump
        // FileQueue and append another ready completion without invalidation.
        auto batch = std::move(ready);
        ready.clear();
        std::exception_ptr failure;
        for (const auto& request : batch) {
            if (request->retired) continue;
            auto* object = request->object.get();
            try {
                if (object->m_buffer != request->destination || object->m_size != request->size ||
                    object->m_file != request->file->handle.get())
                    throw std::logic_error("Native async read fields changed while pending");
                const bool success = !unload && request->result.status == ReadStatus::Success &&
                    request->result.file.bytes.size() == request->size;
                if (!unload && !success) Error(request->result.error.c_str(), request->result.systemError ? request->result.systemError : 38);
                if (success && request->size)
                    std::memcpy(request->destination, request->result.file.bytes.data(), request->size);
                object->m_isRead = 1; object->m_isProcessed = 1;
                request->delivered = true;
                void* raw = success ? object->m_userPostloadCallback : object->m_userPostUnloadCallback;
                if (raw) OsValidateFunctionPointer(raw);
                auto callback = reinterpret_cast<void (*)(void*)>(raw);
                auto* argument = object->m_userArg;
                // Callback may destroy its object, close its file and submit the
                // next read. No object/buffer access is allowed after this call.
                if (callback) callback(argument);
            } catch (...) { if (!failure) failure = std::current_exception(); }
        }
        if (failure) std::rethrow_exception(failure);
    }
};
namespace {
LegacyFiles::State& Current() {
    if (!active) throw std::logic_error("Native legacy file adapter is not installed");
    active->Check(); return *active;
}
void ValidateCallback(void* callback) {
    if (callback) OsValidateFunctionPointer(callback);
}
}
LegacyFiles::LegacyFiles(FileQueue& queue, std::shared_ptr<const Blizzard::Mopaq::Native::Packages> packages)
    : LegacyFiles(queue, [packages](const std::string& path, const Cancellation&) {
        if (!packages) throw std::invalid_argument("Missing authenticated model packages");
        ReadResult result; result.file = packages->Read(path, MaxFile); return result;
    }) {
    if (!packages) { active = nullptr; throw std::invalid_argument("Missing authenticated model packages"); }
}
LegacyFiles::LegacyFiles(FileQueue& queue, Reader reader) : state(new State(queue, std::move(reader))) {
    if (active || !state->reader) throw std::logic_error("Invalid/duplicate native legacy file adapter");
    active = state.get();
}
LegacyFiles::~LegacyFiles() {
    try {
        state->Check();
        if (state->delivering) std::terminate();
        // Failure unwinding retires destinations before releasing handles.
        // Captured Request/file state survives worker completion; callbacks only
        // record ready state and cannot invoke dead model owners.
        while (!state->requests.empty()) state->Retire(state->requests.begin()->second);
        state->files.clear();
        active = nullptr;
    } catch (...) { std::terminate(); }
}
void LegacyFiles::Poll() { state->Check(); state->queue.Poll(); state->Deliver(); }
void LegacyFiles::CheckCanClose() const {
    state->Check();
    if (!state->requests.empty() || !state->files.empty() || state->delivering)
        throw std::logic_error("Close model async objects and file handles before native files");
}
}}

using namespace Services::Native;
// These definitions are selected only in the opt-in native target. The full
// Windows client retains SFile/AsyncFile.cpp and its original runtime ABI.
SFileImpl::SFileImpl(SFILE_TYPE type) : m_type(type), m_stream(nullptr), m_archive(nullptr),
    m_name(nullptr), m_actualName(nullptr), m_size(0), m_data(nullptr), m_position(0),
    m_file(nullptr), unused_0024(0), unused_0028(0), m_lock(nullptr) {}
SFileImpl::~SFileImpl() = default;
int32_t STORMAPI SFile::OpenEx(SArchive* archive, const char* name, uint32_t flags, SFileImpl** output) {
    auto& s = Current();
    if (!output) return s.Error("Missing SFile output");
    *output = nullptr;
    if (archive || !name || !*name || flags > 1) return s.Error("Only local package SFile opens are available");
    auto path = Blizzard::Mopaq::Native::Name(name);
    auto result = std::make_shared<ReadResult>();
    auto reader = s.reader;
    auto ticket = s.queue.Submit([reader, path](const Cancellation& cancel) { return reader(path, cancel); },
        [result](const ReadResult& read) { *result = read; });
    if (!s.queue.Wait(ticket, std::chrono::seconds(60))) {
        s.queue.Cancel(ticket); return s.Error("Native model file open timed out", 1460);
    }
    if (result->status != ReadStatus::Success) return s.Error(result->error.c_str(), result->systemError ? result->systemError : 2);
    if (result->file.bytes.size() > MaxFile) return s.Error("Native model file exceeds local read limit");
    auto file = std::make_shared<OpenFile>();
    file->handle.reset(new SFileImpl(SFILE_MEMORY));
    file->bytes = std::move(result->file.bytes); file->name = path;
    auto* handle = file->handle.get();
    handle->m_size = static_cast<uint32_t>(file->bytes.size());
    handle->m_data = file->bytes.data(); handle->m_name = file->name.data();
    s.files.emplace(handle, file); *output = handle; return 1;
}
int32_t STORMAPI SFileOpenFile(const char* name, SFileImpl** output) { return SFile::OpenEx(nullptr, name, 0, output); }
int32_t STORMAPI SFileCloseFile(SFileImpl* handle) {
    auto& s = Current(); auto file = s.File(handle);
    if (file->asyncOwner) return s.Error("Pending CAsyncObject owns this SFile handle");
    s.files.erase(handle); return 1;
}
uint32_t STORMAPI SFileGetFileSize(SFileImpl* handle, uint32_t* high) {
    auto file = Current().File(handle);
    if (high) *high = 0;
    return static_cast<uint32_t>(file->bytes.size());
}
int32_t STORMAPI SFileReadFile(SFileImpl* handle, void* buffer, uint32_t count, uint32_t* bytesRead, SOVERLAPPED* overlapped, uint32_t unused) {
    auto& s = Current(); auto file = s.File(handle);
    if (bytesRead) *bytesRead = 0;
    if (overlapped || unused || (!buffer && count) || file->asyncOwner || handle->m_position > file->bytes.size())
        return s.Error("Invalid/unavailable native SFile read");
    auto size = static_cast<uint32_t>(std::min<size_t>(count, file->bytes.size() - handle->m_position));
    if (size) std::memcpy(buffer, file->bytes.data() + handle->m_position, size);
    handle->m_position += size;
    if (bytesRead) *bytesRead = size;
    return size == count ? 1 : s.Error("Short local SFile read", 38);
}
int32_t STORMAPI SFile::SetFilePointer(SFileImpl* handle, int32_t distance, int32_t* high, uint32_t method) {
    auto& s = Current(); auto file = s.File(handle);
    if (high || method > 2 || file->asyncOwner) { s.Error("Unavailable native SFile seek"); return -1; }
    int64_t base = method == 0 ? 0 : method == 1 ? handle->m_position : file->bytes.size();
    int64_t position = base + distance;
    if (position < 0 || uint64_t(position) > file->bytes.size()) { s.Error("Invalid native SFile seek extent"); return -1; }
    handle->m_position = static_cast<uint32_t>(position); return static_cast<int32_t>(position);
}
const char* SFile::GetErrorDetails() { return Current().error.c_str(); }
int32_t SFile::IsStreamingMode() { Current(); return 0; } // Mounted local packages only.
int32_t STORMAPI SFile::GetActualFileName(SFileImpl* handle, char* buffer, uint32_t size) {
    auto& s = Current(); auto file = s.File(handle);
    if (!buffer || file->name.size() >= size) return s.Error("Insufficient SFile filename buffer");
    std::memcpy(buffer, file->name.c_str(), file->name.size() + 1); return 1;
}
int32_t CAsyncObject::IsPending() { Current().Object(this); return !m_isCurrent && !m_isRead && !m_isProcessed; }
CAsyncObject* AsyncFileReadAllocObject() {
    auto& s = Current(); auto request = std::make_shared<Request>();
    request->object->m_priority = 126;
    auto* object = request->object.get(); s.requests.emplace(object, request); return object;
}
void AsyncFileReadObject(CAsyncObject* object, int32_t urgent) {
    auto& s = Current(); auto request = s.Object(object);
    auto file = s.File(object->m_file);
    if (request->submitted || file->asyncOwner || (!object->m_buffer && object->m_size) || object->m_queue ||
        object->m_isRead || object->m_isProcessed || object->m_isCurrent || file->handle->m_position > file->bytes.size())
        throw std::invalid_argument("Invalid native model async read");
    ValidateCallback(object->m_userPostloadCallback); ValidateCallback(object->m_userPostUnloadCallback);
    const size_t offset = file->handle->m_position;
    const uint32_t size = object->m_size;
    request->file = file; request->destination = object->m_buffer; request->size = size;
    // Worker captures only immutable file bytes and extents. Destination copy and
    // legacy callbacks are both deferred to owner-thread delivery.
    request->ticket = s.queue.Submit([file, offset, size](const Cancellation&) {
        ReadResult result;
        if (size > file->bytes.size() - offset) {
            result.status = ReadStatus::Failed; result.error = "Short native model async read";
        } else result.file.bytes.assign(file->bytes.begin() + offset, file->bytes.begin() + offset + size);
        return result;
    }, [request, &s](const ReadResult& result) {
        if (request->retired) return;
        request->result = result; request->ready = true; s.ready.push_back(request);
    }, object->m_priority, urgent != 0);
    request->submitted = true; file->asyncOwner = object;
    file->handle->m_position += static_cast<uint32_t>(std::min<size_t>(size, file->bytes.size() - offset));
}
void AsyncFileReadDestroyObject(CAsyncObject* object) { auto& s = Current(); s.Retire(s.Object(object)); }
int32_t AsyncFile::Cancel(CAsyncObject* object, void (*callback)(CAsyncObject*)) {
    ValidateCallback(reinterpret_cast<void*>(callback));
    auto& s = Current(); s.Retire(s.Object(object));
    // Workers never retain destination/object pointers, so immediate retirement
    // always wins on the owner. As in the original return-1 path, the caller
    // frees its buffer and the deferred cleanup callback is not invoked.
    return 1;
}
void DefaultAsyncObjectCleanupCallback(CAsyncObject* object) { AsyncFileReadDestroyObject(object); }
void AsyncFileReadIncreasePriority(CAsyncObject* object) {
    auto& s = Current(); auto request = s.Object(object);
    object->m_priority = object->m_priority > 127 ? 128 : 0;
    if (request->ticket) s.queue.Promote(request->ticket, object->m_priority);
}
void AsyncFileReadWait(CAsyncObject* object) {
    auto& s = Current(); auto request = s.Object(object);
    if (!request->submitted || s.delivering) throw std::logic_error("Invalid/reentrant legacy file wait");
    auto ticket = request->ticket;
    if (!s.queue.Wait(ticket, std::chrono::seconds(60))) throw std::runtime_error("Native model async read timed out");
    s.Deliver();
}
void AsyncFileReadWaitAll() {
    auto& s = Current();
    if (s.delivering) throw std::logic_error("Recursive legacy file wait-all");
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
    for (;;) {
        bool pending = false;
        for (const auto& pair : s.requests) pending |= pair.second->submitted && !pair.second->delivered;
        if (!pending) return;
        if (!s.queue.WaitAll(std::chrono::milliseconds(1000)) && std::chrono::steady_clock::now() >= deadline)
            throw std::runtime_error("Native model wait-all timed out");
        s.Deliver();
        if (std::chrono::steady_clock::now() >= deadline) throw std::runtime_error("Native model wait-all callback chain timed out");
    }
}
void AsyncFileReadUnloadPostList() { auto& s = Current(); s.queue.Poll(); s.Deliver(true); }
int32_t AsyncFileReadPollHandler(const void*, void*) { auto& s = Current(); s.queue.Poll(); s.Deliver(); return 1; }
