// Semantic checks for original Model2's I/O and signed-relative storage contracts.
// Runs in the existing startup executable; all compilation/execution is hosted.
#include "Engine/Source/Services/Portable/LegacyFiles.h"
#include "Engine/Source/Services/AsyncFile.h"
#include "Engine/Source/BlizzardCore/SFile2-Core.h"
#include "Engine/Source/Os/Portable/ModelMemory.h"
#include "Engine/Source/Os/OsMemory.h"
#include "Engine/Source/Model2/M2Data.h"
#include <chrono>
#include <cstring>
#include <functional>
#include <future>
#include <thread>
#include <stdexcept>

using namespace Services::Native;
namespace {
void Check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
struct Completion {
    CAsyncObject* object = nullptr;
    std::thread::id owner = std::this_thread::get_id();
    int loaded = 0, failed = 0;
    std::function<void()> next;
};
void Loaded(void* pointer) {
    auto& completion = *static_cast<Completion*>(pointer);
    Check(std::this_thread::get_id() == completion.owner, "Legacy model callback left owner thread");
    ++completion.loaded;
    auto* object = completion.object;
    completion.object = nullptr;
    AsyncFileReadDestroyObject(object);
    if (completion.next) completion.next();
}
void Failed(void* pointer) {
    auto& completion = *static_cast<Completion*>(pointer);
    ++completion.failed;
    auto* object = completion.object;
    completion.object = nullptr;
    AsyncFileReadDestroyObject(object);
}
int deferredCleanups = 0;
void DeferredCleanup(CAsyncObject*) { ++deferredCleanups; }
void Start(Completion& completion, const char* name, void* destination, uint32_t size, bool urgent = false) {
    SFileImpl* file = nullptr;
    Check(SFile::OpenEx(nullptr, name, 0, &file), "Model fixture member open failed");
    auto* object = AsyncFileReadAllocObject();
    completion.object = object;
    object->m_file = file; object->m_buffer = destination; object->m_size = size;
    object->m_userArg = &completion;
    object->m_userPostloadCallback = reinterpret_cast<void*>(Loaded);
    object->m_userPostUnloadCallback = reinterpret_cast<void*>(Failed);
    object->m_priority = urgent ? 125 : 126;
    AsyncFileReadObject(object, urgent);
}
ReadResult ReadFixture(const std::string& path, const Cancellation&) {
    ReadResult result;
    if (path == Blizzard::Mopaq::Native::Name("model.m2")) result.file.bytes = {1, 2, 3, 4};
    else if (path == Blizzard::Mopaq::Native::Name("model00.skin")) result.file.bytes = {5, 6};
    else if (path == Blizzard::Mopaq::Native::Name("empty.anim")) result.file.bytes = {};
    else { result.status = ReadStatus::Failed; result.error = "Expected missing fixture member"; }
    return result;
}
void ModelMemory() {
    using Array = M2Array<uint32_t>;
    static_assert(sizeof(Array) == 8, "Original M2 relative array wire width changed");
    Check(OsNative::LiveModelAllocations() == 0, "Model allocation registry was not empty");
    Check(!OsNative::AllocModelRoot(0) && !OsNative::AllocModelRoot(65537, 65536), "Invalid model root extent accepted");
    auto* root = static_cast<uint8_t*>(OsNative::AllocModelRoot(4096, 65536));
    Check(root && reinterpret_cast<uintptr_t>(root) % 16 == 0, "Model root reservation/alignment failed");
    auto* array = ::new (root) Array{3, 128};
    const uint32_t values[] = {11, 22, 33};
    std::memcpy(root + 128, values, sizeof(values));
    array->Relocate(root);
    Check(array->Data() == reinterpret_cast<uint32_t*>(root + 128) && (*array)[2] == 33, "Original M2 relocation failed");
    auto* backwards = ::new (root + 256) Array{3, 0};
    backwards->Set(array->Data());
    Check(static_cast<int32_t>(backwards->m_offset) < 0 && (*backwards)[1] == 22, "Signed negative M2 displacement lost");
    const auto previous = array->m_offset;
    bool rejected = false;
    auto distant = (reinterpret_cast<uintptr_t>(array) + uint64_t(INT32_MAX) + 4) & ~uintptr_t(3);
    try { array->Set(reinterpret_cast<uint32_t*>(distant)); } catch (const std::out_of_range&) { rejected = true; }
    Check(rejected && array->m_offset == previous, "Out-of-range M2 target was truncated or changed prior offset");
    rejected = false;
    try { (void)(*array)[3]; } catch (const std::out_of_range&) { rejected = true; }
    Check(rejected, "M2 relative array index escaped its count");
    auto* payload = static_cast<uint8_t*>(OsAllocNear(root + 32, 4096));
    Check(payload && reinterpret_cast<uintptr_t>(payload) % 16 == 0, "Near model payload failed");
    array->Set(reinterpret_cast<uint32_t*>(payload));
    array->Data()[0] = 44;
    Check((*array)[0] == 44, "Original M2 array did not address reserved external payload");
    payload[4095] = 99;
    Check(!OsAllocNear(root, 65536), "Model reservation exhaustion fell back to an unrelated allocation");
    uint32_t unrelated = 0;
    Check(!OsAllocNear(&unrelated, 16), "Unknown model anchor admitted");
    void* foreign = root;
    bool foreignFreeRejected = false;
    std::thread thread([&] {
        foreign = OsAllocNear(root, 16);
        try { OsNative::FreeModelRoot(root); } catch (const std::invalid_argument&) { foreignFreeRejected = true; }
    });
    thread.join();
    Check(!foreign && foreignFreeRejected, "Foreign thread mutated model storage");
    rejected = false;
    try { OsFreeNear(root); } catch (const std::invalid_argument&) { rejected = true; }
    Check(rejected && OsNative::LiveModelAllocations() == 2, "Wrong allocation-kind release corrupted model ownership");
    OsNative::FreeModelRoot(root);
    Check(!OsAllocNear(payload, 16) && payload[4095] == 99, "Retired root admitted allocations or lost retained payload");
    OsFreeNear(payload);
    Check(OsNative::LiveModelAllocations() == 0, "Model reservation/payload owner leaked");
}
void ModelFiles() {
    FileQueue queue;
    {
        LegacyFiles files(queue, ReadFixture);
        SFileImpl* file = reinterpret_cast<SFileImpl*>(uintptr_t(1));
        Check(!SFileOpenFile("missing.anim", &file) && !file, "Missing model file reported success");
        Check(std::strstr(SFile::GetErrorDetails(), "Expected missing"), "Model read failure lost its diagnostic");
        Check(SFileOpenFile("model.m2", &file), "Local SFile open failed");
        uint32_t high = 99, count = 99;
        Check(SFileGetFileSize(file, &high) == 4 && high == 0, "Original model file size width changed");
        uint8_t bytes[8]{};
        Check(!SFileReadFile(file, bytes, sizeof(bytes), &count, nullptr, 0) && count == 4 && bytes[3] == 4,
            "SFile short read lost bytes/count/failure");
        Check(SFile::SetFilePointer(file, -2, nullptr, 2) == 2 && SFileReadFile(file, bytes, 2, &count, nullptr, 0) && bytes[0] == 3,
            "Local model seek/read failed");
        Check(SFile::SetFilePointer(file, -5, nullptr, 2) == -1, "Invalid model seek accepted");
        SFileCloseFile(file);
        // Model completion destroys itself, opens a skin and queues its callback.
        // Opening can pump FileQueue, but cannot recursively invoke this callback.
        Completion model, skin;
        uint8_t modelBytes[4]{}, skinBytes[2]{};
        model.next = [&] { Start(skin, "model00.skin", skinBytes, sizeof(skinBytes), true); };
        Start(model, "model.m2", modelBytes, sizeof(modelBytes));
        Check(!SFileCloseFile(model.object->m_file), "Caller closed a pending async object's file");
        auto* conflicting = AsyncFileReadAllocObject();
        conflicting->m_file = model.object->m_file;
        bool ownershipRejected = false;
        try { AsyncFileReadDestroyObject(conflicting); } catch (const std::logic_error&) { ownershipRejected = true; }
        Check(ownershipRejected, "A second async object closed the first object's pending file");
        conflicting->m_file = nullptr;
        AsyncFileReadDestroyObject(conflicting);
        AsyncFileReadWaitAll();
        Check(model.loaded == 1 && skin.loaded == 1 && !model.failed && !skin.failed && modelBytes[3] == 4 && skinBytes[1] == 6,
            "Model-to-skin callback transaction did not complete exactly once");
        Completion shortRead;
        std::memset(bytes, 0xCC, sizeof(bytes));
        Start(shortRead, "model.m2", bytes, sizeof(bytes));
        AsyncFileReadWait(shortRead.object);
        Check(shortRead.failed == 1 && !shortRead.loaded && bytes[0] == 0xCC && bytes[7] == 0xCC,
            "Short model read wrote destination or used success callback");
        Completion empty;
        Start(empty, "empty.anim", nullptr, 0);
        AsyncFileReadWait(empty.object);
        Check(empty.loaded == 1, "Valid empty model member failed");
        // Queue completions alone do not touch caller memory. Retirement after
        // read completion suppresses destination writes and both callbacks.
        Completion cancelled;
        Start(cancelled, "model.m2", bytes, 4);
        Check(queue.WaitAll(std::chrono::seconds(5)), "Could not stage cancellation fixture");
        Check(bytes[0] == 0xCC && cancelled.loaded == 0, "FileQueue pump invoked legacy consumers");
        Check(AsyncFile::Cancel(cancelled.object, DeferredCleanup) == 1, "Immediate model cancellation did not retire destination");
        cancelled.object = nullptr;
        files.Poll();
        Check(bytes[0] == 0xCC && !cancelled.loaded && !cancelled.failed && !deferredCleanups, "Cancelled model completion escaped retirement");
        Completion unload;
        Start(unload, "model.m2", bytes, 4);
        Check(queue.WaitAll(std::chrono::seconds(5)), "Could not stage unload fixture");
        AsyncFileReadUnloadPostList();
        Check(unload.failed == 1 && !unload.loaded && bytes[0] == 0xCC, "Unload callback copied model bytes or reported success");
        files.CheckCanClose();
    }
    // Pending retirement must remain safe after the bridge itself has closed.
    // A blocked worker makes cancellation timing deterministic.
    std::promise<void> entered, release;
    auto released = release.get_future().share();
    struct Release {
        std::promise<void>& signal;
        bool sent = false;
        void Send() { if (!sent) { signal.set_value(); sent = true; } }
        ~Release() { Send(); }
    } guard{release};
    {
        LegacyFiles files(queue, ReadFixture);
        SFileImpl* file = nullptr;
        Check(SFileOpenFile("model.m2", &file), "Pending model open failed");
        auto* object = AsyncFileReadAllocObject();
        uint8_t* destination = new uint8_t[4];
        object->m_file = file; object->m_buffer = destination; object->m_size = 4;
        auto blocker = queue.Submit([&](const Cancellation&) { entered.set_value(); released.wait(); return ReadResult{}; }, [](const ReadResult&) {});
        Check(entered.get_future().wait_for(std::chrono::seconds(5)) == std::future_status::ready, "Pending model worker gate failed");
        AsyncFileReadObject(object, 0);
        // The bridge destructor retires this outstanding object. Delete the
        // destination now: workers only own their staged bytes.
        delete[] destination;
    }
    guard.Send();
    Check(queue.WaitAll(std::chrono::seconds(5)), "Retired model read did not drain after adapter destruction");
    queue.Shutdown();
}
}
void CheckNativeModelIO() { ModelMemory(); ModelFiles(); }
