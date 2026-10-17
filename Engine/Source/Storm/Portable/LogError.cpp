#include "Engine/Source/Storm/H/storm.h"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <new>

namespace {
struct Log { Log* next; HSLOG handle; FILE* file; uint32_t flags; char* path; };
struct Logs { std::mutex mutex; Log* first = nullptr; uintptr_t sequence = 0; };
Logs& Registry() {
    // Process-lifetime lock remains available to late global destructors. It uses
    // malloc directly, so an allocator failure can report without recursion.
    static Logs* logs = [] {
        void* memory = std::malloc(sizeof(Logs));
        if (!memory) { std::fputs("Cannot initialize core logging\n", stderr); std::abort(); }
        return new (memory) Logs;
    }();
    return *logs;
}
thread_local uint32_t lastError = 0;
std::atomic<int32_t (*)(char*, uint32_t)> callback{nullptr};
Log* Find(HSLOG handle) {
    for (Log* log = Registry().first; log; log = log->next) if (log->handle == handle) return log;
    return nullptr;
}
bool Open(Log& log) {
    if (log.flags & SLOG_FLAG_NOFILE) return false;
    if (log.file) return true;
    std::error_code error;
    auto parent = std::filesystem::path(log.path).parent_path();
    if (!parent.empty()) std::filesystem::create_directories(parent, error);
    if (error) { lastError = error.value(); return false; }
    log.file = std::fopen(log.path, log.flags & SLOG_FLAG_APPEND ? "ab" : "wb");
    if (!log.file) lastError = errno;
    return log.file != nullptr;
}
}
void STORMAPI SLogInitialize() { Registry(); }
int32_t STORMAPI SLogCreate(const char* filename, uint32_t flags, HSLOG* result) {
    if (!result) { lastError = STORM_ERROR_INVALID_PARAMETER; return 0; }
    *result = nullptr;
    if (!filename || !*filename) { lastError = STORM_ERROR_INVALID_PARAMETER; return 0; }
    auto& registry = Registry();
    std::lock_guard<std::mutex> lock(registry.mutex);
    auto log = static_cast<Log*>(std::calloc(1, sizeof(Log)));
    if (!log) { lastError = STORM_ERROR_NOT_ENOUGH_MEMORY; return 0; }
    log->path = static_cast<char*>(std::malloc(std::strlen(filename) + 1));
    if (!log->path) { std::free(log); lastError = STORM_ERROR_NOT_ENOUGH_MEMORY; return 0; }
    std::strcpy(log->path, filename);
    log->flags = flags;
    if ((flags & SLOG_FLAG_OPENFILE) && !(flags & SLOG_FLAG_NOFILE) && !Open(*log)) {
        std::free(log->path); std::free(log); return 0;
    }
    log->handle = reinterpret_cast<HSLOG>(++registry.sequence);
    log->next = registry.first;
    registry.first = log;
    *result = log->handle;
    return 1;
}
void STORMAPI SLogVWrite(HSLOG handle, const char* format, va_list args) {
    if (!format) return;
    auto& registry = Registry();
    std::lock_guard<std::mutex> lock(registry.mutex);
    Log* log = Find(handle);
    if (log && Open(*log)) {
        if (std::vfprintf(log->file, format, args) < 0 || std::fputc('\n', log->file) == EOF)
            lastError = errno;
    }
}
void SLogWrite(HSLOG handle, const char* format, ...) {
    va_list args; va_start(args, format); SLogVWrite(handle, format, args); va_end(args);
}
void STORMAPI SLogFlush(HSLOG handle) {
    auto& registry = Registry();
    std::lock_guard<std::mutex> lock(registry.mutex);
    Log* log = Find(handle);
    if (log && log->file && std::fflush(log->file)) lastError = errno;
}
void STORMAPI SLogFlushAll() {
    auto& registry = Registry();
    std::lock_guard<std::mutex> lock(registry.mutex);
    for (Log* log = registry.first; log; log = log->next)
        if (log->file && std::fflush(log->file)) lastError = errno;
}
void STORMAPI SLogClose(HSLOG handle) {
    auto& registry = Registry();
    std::lock_guard<std::mutex> lock(registry.mutex);
    for (Log** current = &registry.first; *current; current = &(*current)->next) {
        if ((*current)->handle != handle) continue;
        Log* log = *current; *current = log->next;
        if (log->file && std::fclose(log->file)) lastError = errno;
        std::free(log->path); std::free(log); break;
    }
}
void STORMAPI SLogDestroy() {
    // Lifecycle operation: callers have stopped workers before destroying logs.
    while (Registry().first) SLogClose(Registry().first->handle);
}
uint32_t STORMAPI SErrGetLastError() { return lastError; }
void STORMAPI SErrSetLastError(uint32_t error) { lastError = error; }
void STORMAPI SErrSetLogCallback(int32_t (STORMAPI* function)(char*, uint32_t)) { callback.store(function); }
int32_t STORMAPI SErrDestroy() { callback.store(nullptr); return 1; }
int32_t STORMAPI SErrDisplayError(uint32_t error, const char* file, int32_t line,
                                  const char* description, int32_t recoverable, uint32_t code, uint32_t) {
    lastError = error;
    char context[1024] = {};
    if (auto function = callback.load()) function(context, sizeof(context));
    std::fprintf(stderr, "%s\n%s:%d: error 0x%08x: %s\n", context,
                 file ? file : "<unknown>", line, error, description ? description : "<none>");
    std::fflush(stderr);
    if (!recoverable) StormExitProcess(static_cast<int32_t>(code));
    return 1;
}
[[noreturn]] void SErrDisplayAppFatalMessage(const char* message, const char* file, int32_t line) {
    SErrDisplayError(STORM_ERROR_FATAL, file, line, message, 0, 1, 0);
    std::abort();
}
[[noreturn]] void StormExitProcess(int32_t code) { std::_Exit(code); }
