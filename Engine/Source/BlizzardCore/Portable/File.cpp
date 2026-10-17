#include "Engine/Source/BlizzardCore/Portable/FilePlatform.h"
#include "Engine/Source/Storm/H/storm.h"
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <memory>
#include <mutex>
#include <new>

namespace fs = std::filesystem;
namespace NativeFile {
bool Fail(int error) {
    errno = error;
    uint32_t code;
    switch (error) {
        case ENOENT: code = 2; break;
        case ENOTDIR: code = 3; break;
        case EACCES: case EPERM: code = 5; break;
        case EBADF: code = 6; break;
        case ENOMEM: code = 8; break;
        case EROFS: code = 19; break;
        case ENOTSUP: code = 50; break;
        case EINVAL: code = 87; break;
        case ENOSPC: code = 112; break;
        case EEXIST: code = 183; break;
        case ENAMETOOLONG: code = 206; break;
        case EOVERFLOW: code = 534; break;
        case EIO: code = 1117; break;
        default: code = 31; break;
    }
    SErrSetLastError(code);
    return false;
}
bool Fail(const std::error_code& error) { return Fail(error.default_error_condition().value()); }

bool HostPath(const char* input, fs::path& result) {
    if (!input || !*input) return Fail(EINVAL);
    // Reject malformed UTF-8 before a host library can accept it as raw bytes or
    // replace characters. Host filename normalization/case rules stay separate.
    const auto* p = reinterpret_cast<const unsigned char*>(input);
    while (*p) {
        uint32_t scalar = *p++;
        if (scalar < 0x80) continue;
        unsigned extra;
        uint32_t minimum;
        if (scalar >= 0xC2 && scalar <= 0xDF) { extra = 1; minimum = 0x80; scalar &= 0x1F; }
        else if (scalar >= 0xE0 && scalar <= 0xEF) { extra = 2; minimum = 0x800; scalar &= 0x0F; }
        else if (scalar >= 0xF0 && scalar <= 0xF4) { extra = 3; minimum = 0x10000; scalar &= 7; }
        else return Fail(EINVAL);
        for (unsigned i = 0; i < extra; ++i) {
            if ((*p & 0xC0) != 0x80) return Fail(EINVAL);
            scalar = (scalar << 6) | (*p++ & 0x3F);
        }
        if (scalar < minimum || scalar > 0x10FFFF || (scalar >= 0xD800 && scalar <= 0xDFFF)) return Fail(EINVAL);
    }
    std::string text(input);
    std::replace(text.begin(), text.end(), '\\', '/');
    result = fs::u8path(text);
    return true;
}
}

namespace {
struct State {
    Blizzard::File::StreamRecord record{};
    NativeFile::Handle handle = NativeFile::Invalid;
    std::string path;
    std::mutex mutex;
    int64_t position = 0;
    ~State() { if (handle != NativeFile::Invalid) NativeFile::Close(handle); }
};
State* Get(Blizzard::File::StreamRecord* file) {
    if (!file || !file->m_fileHandle) { NativeFile::Fail(EBADF); return nullptr; }
    return static_cast<State*>(file->m_fileHandle);
}
bool Refresh(State& state, Blizzard::File::FileInfo& info) {
    if (!NativeFile::Info(state.handle, info)) return false;
    info.m_path = state.path.data();
    return true;
}
bool Transfer(Blizzard::File::StreamRecord* file, void* data, int32_t* count, int64_t offset, bool positioned, bool write) {
    if (!count) return NativeFile::Fail(EINVAL);
    const int32_t request = *count;
    *count = 0;
    if (request < 0 || (request && !data)) return NativeFile::Fail(EINVAL);
    State* state = Get(file);
    if (!state) return false;
    std::lock_guard<std::mutex> lock(state->mutex);
    if (!(file->m_mode & (write ? 2 : 1))) return NativeFile::Fail(EACCES);
    if (!positioned) offset = state->position;
    if (offset < 0 || offset > INT64_MAX - request) return NativeFile::Fail(EOVERFLOW);
    if (!request) return true;
    *count = request;
    if (!NativeFile::Transfer(state->handle, data, offset, *count, write)) return false;
    if (!positioned) state->position += *count;
    if (write) file->m_infoValid = false;
    return true;
}
bool Position(State& state, int64_t delta, int32_t whence, int64_t& result) {
    if (whence < 0 || whence > 2) return NativeFile::Fail(EINVAL);
    int64_t base = whence == 1 ? state.position : 0;
    if (whence == 2) {
        Blizzard::File::FileInfo info{};
        if (!Refresh(state, info)) return false;
        base = info.m_size;
    }
    if ((delta > 0 && base > INT64_MAX - delta) || delta < -base) return NativeFile::Fail(EOVERFLOW);
    result = base + delta;
    return true;
}
bool Output(const std::string& text, char* buffer, int32_t size) {
    if (!buffer || size <= 0) return NativeFile::Fail(EINVAL);
    *buffer = 0;
    if (text.size() >= static_cast<size_t>(size)) return NativeFile::Fail(ENAMETOOLONG);
    std::memcpy(buffer, text.c_str(), text.size() + 1);
    return true;
}
}

namespace Blizzard { namespace File {
bool Native::Open(const char* name, int32_t access, Disposition disposition, StreamRecord*& file) {
    file = nullptr;
    if (disposition < Disposition::CreateNew || disposition > Disposition::TruncateExisting) return NativeFile::Fail(EINVAL);
    if (!(access & 3) || (access & ~15)) return NativeFile::Fail(EINVAL);
    if ((disposition == Disposition::CreateAlways || disposition == Disposition::TruncateExisting) && !(access & 2))
        return NativeFile::Fail(EACCES);
    fs::path path;
    if (!NativeFile::HostPath(name, path)) return false;
    try {
        auto state = std::make_unique<State>();
        state->path = path.u8string();
        if (!NativeFile::Open(path, access, disposition, state->handle)) return false;
        state->record.m_mode = access;
        state->record.m_fileHandle = state.get();
        state->record.m_pathname = state->path.data();
        if (!Refresh(*state, state->record.m_info)) return false;
        state->record.m_infoValid = true;
        file = &state.release()->record;
        return true;
    } catch (const std::bad_alloc&) { return NativeFile::Fail(ENOMEM); }
}
bool Open(const char* name, int32_t flags, StreamRecord*& file) {
    using D = Native::Disposition;
    file = nullptr;
    // Direct-I/O, temporary and streaming flags await package-layer integration.
    if (flags & ~0x1F0F) return NativeFile::Fail(ENOTSUP);
    D disposition = flags & (0x100 | 0x400) ? ((flags & 0x800) ? D::CreateNew : D::CreateAlways) :
                    flags & 0x800 ? D::CreateNew : flags & 0x1000 ? D::OpenExisting : D::OpenAlways;
    if (!Native::Open(name, flags & 15, disposition, file)) return false;
    if ((flags & 0x200) && !SetPos(file, 0, 2)) { Close(file); file = nullptr; return false; }
    return true;
}
bool Close(StreamRecord* file) {
    State* state = Get(file);
    if (!state) return false;
    const auto handle = state->handle;
    state->handle = NativeFile::Invalid;
    const bool result = NativeFile::Close(handle);
    delete state;
    return result;
}
bool Flush(StreamRecord* file) {
    State* state = Get(file);
    if (!state) return false;
    std::lock_guard<std::mutex> lock(state->mutex);
    return NativeFile::Flush(state->handle);
}
bool Read(StreamRecord* file, void* data, int32_t* count) { return Transfer(file, data, count, 0, false, false); }
bool Read(StreamRecord* file, void* data, int64_t offset, int32_t* count) { return Transfer(file, data, count, offset, true, false); }
bool Write(StreamRecord* file, const void* data, int32_t* count) { return Transfer(file, const_cast<void*>(data), count, 0, false, true); }
bool Write(StreamRecord* file, const void* data, int64_t offset, int32_t* count) { return Transfer(file, const_cast<void*>(data), count, offset, true, true); }
bool GetPos(StreamRecord* file, int64_t* offset) {
    if (!offset) return NativeFile::Fail(EINVAL);
    State* state = Get(file);
    if (!state) return false;
    std::lock_guard<std::mutex> lock(state->mutex);
    *offset = state->position;
    return true;
}
bool SetPos(StreamRecord* file, int64_t offset, int32_t whence) {
    State* state = Get(file);
    if (!state) return false;
    std::lock_guard<std::mutex> lock(state->mutex);
    int64_t position;
    if (!Position(*state, offset, whence, position)) return false;
    state->position = position;
    return true;
}
bool SetEOF(StreamRecord* file, int64_t offset, int32_t whence) {
    State* state = Get(file);
    if (!state) return false;
    std::lock_guard<std::mutex> lock(state->mutex);
    if (!(file->m_mode & 2)) return NativeFile::Fail(EACCES);
    int64_t end;
    if (!Position(*state, offset, whence, end) || !NativeFile::Resize(state->handle, end)) return false;
    file->m_infoValid = false;
    return true;
}
bool GetFileInfo(StreamRecord* file, FileInfo* info) {
    if (!info) return NativeFile::Fail(EINVAL);
    State* state = Get(file);
    if (!state) return false;
    std::lock_guard<std::mutex> lock(state->mutex);
    return Refresh(*state, *info);
}
FileInfo* GetFileInfo(StreamRecord* file) {
    if (!GetFileInfo(file, file ? &file->m_info : nullptr)) return nullptr;
    file->m_infoValid = true;
    return &file->m_info;
}
bool GetFileInfo(const char* name, FileInfo* info) {
    if (!info) return NativeFile::Fail(EINVAL);
    fs::path path;
    return NativeFile::HostPath(name, path) && NativeFile::Info(path, *info);
}
int32_t Exists(const char* name) { FileInfo info{}; return GetFileInfo(name, &info) ? info.m_existence : 0; }
bool IsDirectory(const char* name) { return Exists(name) == 2; }
bool IsFile(const char* name) { FileInfo info{}; return GetFileInfo(name, &info) && info.m_regularFile; }
bool CreateDirectory(const char* name, bool recursive) {
    fs::path path;
    if (!NativeFile::HostPath(name, path)) return false;
    std::error_code error;
    const bool made = recursive ? fs::create_directories(path, error) : fs::create_directory(path, error);
    if (error) return NativeFile::Fail(error);
    if (made || fs::is_directory(path, error)) return true;
    return NativeFile::Fail(error ? error : std::make_error_code(std::errc::not_a_directory));
}
bool RemoveDirectory(const char* name) {
    fs::path path;
    if (!NativeFile::HostPath(name, path)) return false;
    std::error_code error;
    if (fs::is_symlink(path, error) || !fs::is_directory(path, error)) return NativeFile::Fail(ENOTDIR);
    if (error) return NativeFile::Fail(error);
    return fs::remove(path, error) || NativeFile::Fail(error ? error : std::make_error_code(std::errc::no_such_file_or_directory));
}
bool Delete(const char* name) {
    fs::path path;
    if (!NativeFile::HostPath(name, path)) return false;
    std::error_code error;
    if (!fs::is_symlink(path, error) && fs::is_directory(path, error)) return NativeFile::Fail(EISDIR);
    if (error) return NativeFile::Fail(error);
    return fs::remove(path, error) || NativeFile::Fail(error ? error : std::make_error_code(std::errc::no_such_file_or_directory));
}
bool RemoveDirectoryAndContents(const char* name, bool removeReadOnly) {
    // Clearing attributes recursively requires a separately qualified policy.
    if (removeReadOnly) return NativeFile::Fail(ENOTSUP);
    fs::path path;
    if (!NativeFile::HostPath(name, path)) return false;
    std::error_code error;
    const auto status = fs::symlink_status(path, error);
    if (error) return NativeFile::Fail(error);
    if (!fs::is_directory(status)) return NativeFile::Fail(ENOTDIR);
    // remove_all removes symlinks encountered in the tree without following them.
    fs::remove_all(path, error);
    return !error || NativeFile::Fail(error);
}
bool Move(const char* src, const char* dst) {
    fs::path from, to;
    return NativeFile::HostPath(src, from) && NativeFile::HostPath(dst, to) && NativeFile::Move(from, to);
}
bool Copy(const char* src, const char* dst, bool overwrite) {
    fs::path from, to;
    if (!NativeFile::HostPath(src, from) || !NativeFile::HostPath(dst, to)) return false;
    std::error_code error;
    const bool copied = fs::copy_file(from, to, overwrite ? fs::copy_options::overwrite_existing : fs::copy_options::none, error);
    return copied || NativeFile::Fail(error ? error : std::make_error_code(std::errc::io_error));
}
bool ProcessDirFast(const char* name, void* cookie, ProcessDirCallback callback, bool symlinksAsFiles) {
    if (!callback) return NativeFile::Fail(EINVAL);
    fs::path path;
    if (!NativeFile::HostPath(name, path)) return false;
    std::error_code error;
    fs::directory_iterator current(path, error), end;
    if (error) return NativeFile::Fail(error);
    const auto parent = path.u8string();
    while (current != end) {
        const auto filename = current->path().filename().u8string();
        bool directory = false;
        if (!(symlinksAsFiles && current->is_symlink(error))) directory = current->is_directory(error);
        if (error) return NativeFile::Fail(error);
        const ProcessDirParms parms{parent.c_str(), filename.c_str(), cookie, directory, {}};
        if (!callback(parms)) return false;
        current.increment(error);
        if (error) return NativeFile::Fail(error);
    }
    return true;
}
bool GetWorkingDirectory(char* buffer, int32_t size) {
    std::error_code error;
    const auto path = fs::current_path(error);
    return error ? NativeFile::Fail(error) : Output(path.u8string(), buffer, size);
}
bool SetWorkingDirectory(const char* name) {
    fs::path path;
    if (!NativeFile::HostPath(name, path)) return false;
    std::error_code error;
    fs::current_path(path, error);
    return !error || NativeFile::Fail(error);
}
bool MakeAbsolutePath(const char* name, char* result, int32_t size, bool canonicalize) {
    fs::path path;
    if (!NativeFile::HostPath(name, path)) return false;
    std::error_code error;
    path = fs::absolute(path, error);
    if (!error && canonicalize) path = fs::weakly_canonical(path, error);
    return error ? NativeFile::Fail(error) : Output(path.u8string(), result, size);
}
bool IsAbsolutePath(const char* name) { fs::path path; return NativeFile::HostPath(name, path) && path.is_absolute(); }
int32_t GetRootChars(const char* name) {
    fs::path path;
    return NativeFile::HostPath(name, path) ? static_cast<int32_t>(path.root_path().u8string().size()) : 0;
}
bool SetAttributes(const char* name, int32_t attributes) {
    fs::path path;
    return NativeFile::HostPath(name, path) && NativeFile::Attributes(path, attributes);
}
bool SimpleGlob(const char* name, const char* pattern) {
    if (!name || !pattern) return NativeFile::Fail(EINVAL);
    const auto fold = [](unsigned char c) { return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c; };
    const char* star = nullptr;
    const char* retry = nullptr;
    while (*name) {
        if (*pattern == '*') { star = ++pattern; retry = name; continue; }
        const bool escaped = *pattern == '\\' && pattern[1];
        const char literal = escaped ? pattern[1] : *pattern;
        if ((!escaped && literal == '?') || (literal && fold(literal) == fold(*name))) {
            ++name;
            pattern += escaped ? 2 : 1;
        } else if (star) { pattern = star; name = ++retry; }
        else return false;
    }
    while (*pattern == '*') ++pattern;
    return !*pattern;
}
}}
