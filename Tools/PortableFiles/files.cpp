#include "Engine/Source/BlizzardCore/Source/System/File/File.h"
#include "Engine/Source/Os/OsFile.h"
#include "Engine/Source/Os/OsSecureRandom.h"
#include "Engine/Source/Storm/H/storm.h"
#include <array>
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>
#if !defined(WOWDECOMP_OS_WINDOWS)
#include <sys/stat.h>
#endif

namespace fs = std::filesystem;
namespace F = Blizzard::File;
namespace N = Blizzard::File::Native;
#if defined(WOWDECOMP_FILES_FAULT_FIXTURES)
void SetReadFault(int fault);
void SetWriteFault(int fault);
#endif
#if defined(WOWDECOMP_OS_WINDOWS)
bool PrepareSparseFixture(const fs::path& path);
#endif
namespace {
void Check(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "FAIL: %s (Storm error %u, errno %d)\n", message, SErrGetLastError(), errno);
        std::exit(1);
    }
}
std::string Name(const fs::path& path) { return path.u8string(); }
struct Directory {
    fs::path path;
    Directory() {
        std::array<uint8_t, 16> random{};
        OsSecureRandom(random.data(), static_cast<uint32_t>(random.size()));
        std::string suffix;
        for (uint8_t byte : random) { suffix += "0123456789abcdef"[byte >> 4]; suffix += "0123456789abcdef"[byte & 15]; }
        path = fs::temp_directory_path() / ("wowdecomp-files-" + suffix);
        Check(fs::create_directory(path), "create isolated synthetic fixture directory");
    }
    ~Directory() { std::error_code error; fs::remove_all(path, error); }
};
struct File {
    F::StreamRecord* value = nullptr;
    File(const fs::path& path, int32_t mode) { Check(F::Open(Name(path).c_str(), mode, value), "open fixture"); }
    ~File() { if (value) F::Close(value); }
    void Close() { Check(F::Close(value), "close fixture"); value = nullptr; }
};
void Save(const fs::path& path, const std::string& text) {
    File file(path, 3 | 12 | 0x400);
    int32_t count = static_cast<int32_t>(text.size());
    Check(F::Write(file.value, text.data(), &count) && count == static_cast<int32_t>(text.size()), "save synthetic text");
    Check(F::Flush(file.value), "flush synthetic text");
}
std::string Load(const fs::path& path) {
    File file(path, 1 | 12 | 0x1000);
    F::FileInfo info{};
    Check(F::GetFileInfo(file.value, &info) && info.m_size < 10000, "small fixture size");
    std::string text(static_cast<size_t>(info.m_size), '\0');
    int32_t count = static_cast<int32_t>(text.size());
    Check(F::Read(file.value, text.data(), &count) && count == static_cast<int32_t>(text.size()), "load complete small fixture");
    return text;
}
int64_t Position(F::StreamRecord* file) { int64_t pos = -1; Check(F::GetPos(file, &pos), "get position"); return pos; }
void Creation(const fs::path& root) {
    const std::string name = Name(root / "creation.bin");
    auto open = [&](uint32_t access, uint32_t disposition) { return OsCreateFile(name.c_str(), access, 3, disposition, OS_FILE_ATTRIBUTE_NORMAL, 0); };
    Check(open(OS_GENERIC_READ, 3) == OS_INVALID_FILE, "OPEN_EXISTING does not create");
    Check(open(OS_GENERIC_WRITE, 5) == OS_INVALID_FILE && !fs::exists(root / "creation.bin"), "TRUNCATE_EXISTING does not create");
    auto file = open(OS_GENERIC_READ | OS_GENERIC_WRITE, 1);
    Check(file != OS_INVALID_FILE, "CREATE_NEW creates");
    uint32_t count = 99;
    Check(OsWriteFile(file, "abcdef", 6, &count) && count == 6, "Os write byte count");
    Check(OsGetFileSize(file) == 6 && OsCloseFile(file), "Os size/close");
    Check(open(OS_GENERIC_WRITE, 1) == OS_INVALID_FILE && Load(fs::u8path(name)) == "abcdef", "CREATE_NEW cannot truncate");
    file = open(OS_GENERIC_READ, 4);
    Check(file != OS_INVALID_FILE && OsGetFileSize(file) == 6 && OsCloseFile(file), "OPEN_ALWAYS preserves existing data");
    Check(open(OS_GENERIC_READ, 5) == OS_INVALID_FILE && Load(fs::u8path(name)) == "abcdef", "read-only truncation rejected before mutation");
    file = open(OS_GENERIC_WRITE, 5);
    Check(file != OS_INVALID_FILE && OsGetFileSize(file) == 0 && OsCloseFile(file), "TRUNCATE_EXISTING truncates");
    Save(fs::u8path(name), "again");
    file = open(OS_GENERIC_WRITE, 2);
    Check(file != OS_INVALID_FILE && OsGetFileSize(file) == 0 && OsCloseFile(file), "CREATE_ALWAYS truncates");
    Check(OsDeleteFile(name.c_str()), "delete creation fixture");
    file = open(OS_GENERIC_READ | OS_GENERIC_WRITE, 4);
    Check(file != OS_INVALID_FILE && OsCloseFile(file), "OPEN_ALWAYS creates missing file");
    F::StreamRecord* unsupported = reinterpret_cast<F::StreamRecord*>(1);
    const auto missing = Name(root / "unsupported.bin");
    Check(!F::Open(missing.c_str(), 3 | 0x40, unsupported) && !unsupported && !fs::exists(fs::u8path(missing)), "unsupported direct I/O fails before creation");
    Check(OsCreateFile(missing.c_str(), OS_GENERIC_WRITE, 3, 2, 0x20000000, 0) == OS_INVALID_FILE, "Os direct I/O is explicitly unavailable");
    auto anyType = OsCreateFile(name.c_str(), OS_GENERIC_READ, 3, OS_OPEN_EXISTING, OS_FILE_ATTRIBUTE_NORMAL, OS_FILE_TYPE_ANY);
    Check(anyType != OS_INVALID_FILE, "Original ANY file-type hint must allow regular files");
    Check(OsCloseFile(anyType), "Close original ANY file-type handle");
    Check(OsCreateFile(missing.c_str(), OS_GENERIC_WRITE, 3, OS_CREATE_ALWAYS, OS_FILE_ATTRIBUTE_NORMAL, 1) == OS_INVALID_FILE,
          "Specific extended file types remain unavailable");
    Check(!N::Open(missing.c_str(), 3, static_cast<N::Disposition>(999), unsupported) && !unsupported && !fs::exists(fs::u8path(missing)), "invalid disposition fails before file creation");
}
void Transfers(const fs::path& root) {
    const auto path = root / "transfers.bin";
    Save(path, "abcdefghij");
    File file(path, 3 | 12 | 0x1000);
    Check(F::SetPos(file.value, 3, 0), "set stream cursor");
    std::array<char, 16> data{};
    int32_t count = 4;
    Check(F::Read(file.value, data.data(), 1, &count) && count == 4 && !std::memcmp(data.data(), "bcde", 4), "positioned read bytes");
    Check(Position(file.value) == 3, "positioned read preserves cursor");
    count = 2;
    Check(F::Write(file.value, "XY", 7, &count) && count == 2 && Position(file.value) == 3, "positioned write preserves cursor");
    Check(F::SetPos(file.value, -2, 2), "seek relative to end");
    data.fill('!'); count = 8;
    Check(F::Read(file.value, data.data(), &count) && count == 2 && data[0] == 'Y' && data[1] == 'j' && data[2] == '!', "short EOF read never copies untouched bytes");
    count = 4;
    Check(F::Read(file.value, data.data(), &count) && count == 0 && Position(file.value) == 10, "EOF is successful zero-byte read");
    count = 0;
    Check(F::Read(file.value, nullptr, &count), "zero-byte null read");
    count = -1;
    Check(!F::Read(file.value, data.data(), &count) && count == 0, "negative count rejected and output cleared");
    count = 2;
    Check(!F::Read(file.value, data.data(), INT64_MAX, &count) && count == 0 && Position(file.value) == 10, "offset extent overflow rejected without cursor mutation");
    Check(!F::SetPos(file.value, INT64_MIN, 1) && Position(file.value) == 10, "negative seek cannot wrap");
    Check(F::SetEOF(file.value, 4, 0) && Position(file.value) == 10, "truncate preserves logical cursor");
    F::FileInfo info{};
    Check(F::GetFileInfo(file.value, &info) && info.m_size == 4, "metadata refreshed after resize");
    const int64_t large = int64_t{5} << 30;
#if defined(WOWDECOMP_OS_WINDOWS)
    Check(PrepareSparseFixture(path), "prepare sparse NTFS fixture before extending");
#endif
    Check(F::SetEOF(file.value, large, 0), "sparse size beyond 32 bits");
    Check(F::GetFileInfo(file.value, &info) && info.m_size == large, "64-bit file metadata");
    uint64_t listedSize = 0;
    auto listing = [](OS_FILE_DATA& entry, void* cookie) { *static_cast<uint64_t*>(cookie) = entry.size; return 0; };
    Check(OsFileList(Name(root).c_str(), "transfers.bin", listing, &listedSize, 1) && listedSize == static_cast<uint64_t>(large), "Os enumeration retains file size beyond 4 GiB");
    count = 2;
    Check(F::Write(file.value, "64", large - 2, &count) && count == 2, "positioned write beyond 4 GiB");
    count = 2;
    Check(F::Read(file.value, data.data(), large - 2, &count) && count == 2 && !std::memcmp(data.data(), "64", 2), "64-bit positioned read");
    Check(F::SetEOF(file.value, 0, 0), "release sparse fixture extent");
    file.Close();
    File readonly(path, 1 | 12 | 0x1000);
    count = 2;
    Check(!F::Write(readonly.value, "no", &count) && count == 0 && Position(readonly.value) == 0, "read-only stream rejects writes");
}
void Concurrent(const fs::path& root) {
    const auto path = root / "concurrent.bin";
    const std::string text = "0123456789abcdefghijklmnopqrstuvwxyz";
    Save(path, text);
    File file(path, 1 | 12 | 0x1000);
    std::atomic<bool> valid{true};
    std::vector<std::thread> readers;
    for (unsigned worker = 0; worker < 4; ++worker) readers.emplace_back([&, worker] {
        for (unsigned i = 0; i < 200; ++i) {
            const size_t offset = (i + worker) % (text.size() - 4);
            char bytes[4]; int32_t count = 4;
            if (!F::Read(file.value, bytes, static_cast<int64_t>(offset), &count) || count != 4 || std::memcmp(bytes, text.data() + offset, 4)) valid = false;
        }
    });
    for (auto& thread : readers) thread.join();
    Check(valid && Position(file.value) == 0, "concurrent positioned reads preserve content and shared cursor");
    SErrSetLastError(12345);
    std::thread failure([&] { F::FileInfo info{}; Check(!F::GetFileInfo(Name(root / "missing").c_str(), &info), "worker error fixture"); });
    failure.join();
    Check(SErrGetLastError() == 12345, "file errors remain thread-local");
    Save(root / "move-a", "a"); Save(root / "move-b", "b");
    std::atomic<bool> start{false}; bool a = false, b = false;
    auto move = [&](const char* src, bool& success) { while (!start.load()) std::this_thread::yield(); success = F::Move(Name(root / src).c_str(), Name(root / "move-to").c_str()); };
    std::thread first(move, "move-a", std::ref(a)), second(move, "move-b", std::ref(b));
    start = true; first.join(); second.join();
    Check(a != b && Load(root / "move-to") == (a ? "a" : "b"), "competing no-replace moves have exactly one winner");
    Check(fs::exists(root / (a ? "move-b" : "move-a")), "losing no-replace move preserves source");
}
void Paths(const fs::path& root) {
    const auto data = root / "Data";
    Check(F::CreateDirectory(Name(data / "DBFilesClient").c_str(), true), "create synthetic data root");
    Save(data / "DBFilesClient" / "Example.DBC", "original fixture");
    const std::string unicode = u8"caf\u00e9-\U0001f409.txt";
    Save(data / fs::u8path(unicode), "unicode");
    N::Paths paths;
    Check(!N::ConfigurePaths(Name(data).c_str(), Name(data / "WTF").c_str(), Name(root / "Cache").c_str(), paths) && !fs::exists(data / "WTF"), "reject writable root inside data before creation");
    Check(N::ConfigurePaths(Name(data).c_str(), Name(root / "Settings").c_str(), Name(root / "Cache").c_str(), paths), "configure separate data/settings/cache roots");
    Check(F::SetAttributes(Name(data).c_str(), 1 | 0x40), "mark data root read-only");
    std::string resolved;
    Check(N::ResolveDataPath(paths, "dbfilesclient\\example.dbc", resolved) && Load(fs::u8path(resolved)) == "original fixture", "host separator conversion and component case lookup");
    F::StreamRecord* stream = nullptr;
    Check(N::OpenData(paths, unicode.c_str(), stream), "Unicode data path");
    int32_t count = 1;
    Check(!F::Write(stream, "!", &count) && count == 0 && F::Close(stream), "data helper supplies read-only stream");
    for (const char* invalid : {"../secret", "/absolute", "C:drive", "dir/../../secret", "./alias", "bad\xc0\xaf", "bad\xed\xa0\x80", "bad\xf4\x90\x80\x80", "bad\xe2"}) {
        Check(!N::ResolveUserPath(paths, N::UserRoot::Settings, invalid, resolved) && resolved.empty(), "invalid relative/UTF-8 path rejected");
    }
    Check(N::ResolveUserPath(paths, N::UserRoot::Settings, "Config.wtf", resolved), "resolve writable settings");
    Save(fs::u8path(resolved), "SET gxWindow \"1\"\n");
    Check(Load(fs::u8path(resolved)) == "SET gxWindow \"1\"\n", "settings save/reload outside data");
    Check(Load(data / "DBFilesClient" / "Example.DBC") == "original fixture" && !fs::exists(data / "Config.wtf"), "settings leave data unchanged");
    Check(F::SetAttributes(Name(data).c_str(), 0x40), "restore synthetic data root permissions for fixture cleanup");
    Save(data / "Case", "upper");
    auto collision = OsCreateFile(Name(data / "case").c_str(), OS_GENERIC_WRITE, 3, 1, OS_FILE_ATTRIBUTE_NORMAL, 0);
    if (collision != OS_INVALID_FILE) {
        Check(OsCloseFile(collision), "close collision fixture");
        Check(!N::ResolveDataPath(paths, "Case", resolved) && resolved.empty() && SErrGetLastError() == 183, "case collision rejected even for exact spelling");
    } else {
        Check(fs::equivalent(data / "Case", data / "case"), "case-insensitive host aliases are the same file");
        std::puts("SKIP: distinct case-collision fixture unavailable on this filesystem");
    }
    std::error_code error;
    fs::create_directory_symlink(root / "Settings", data / "outside", error);
    if (!error) {
        Check(!N::ResolveDataPath(paths, "outside/Config.wtf", resolved), "data symlink escape rejected");
        fs::create_directory_symlink(data, root / "Settings" / "outside", error);
        Check(!error && !N::ResolveUserPath(paths, N::UserRoot::Settings, "outside/file", resolved), "settings symlink escape rejected");
        Check(!N::ConfigurePaths(Name(data).c_str(), Name(root / "Settings" / "outside").c_str(), Name(root / "Cache").c_str(), paths), "overlapping aliased root rejected");
    } else std::puts("SKIP: symlink fixture unavailable on this runner/filesystem");
}
void FileOperations(const fs::path& root) {
#if !defined(WOWDECOMP_OS_WINDOWS)
    const auto pipe = Name(root / "not-a-file");
    Check(mkfifo(pipe.c_str(), 0600) == 0, "create nonregular-file fixture");
    F::StreamRecord* special = nullptr;
    Check(!F::Open(pipe.c_str(), 1 | 0x1000, special) && !special, "reject FIFO without blocking on open");
#endif
    Save(root / "source", "source"); Save(root / "destination", "sentinel");
    Check(!F::Copy(Name(root / "source").c_str(), Name(root / "destination").c_str(), false) && Load(root / "destination") == "sentinel", "no-overwrite copy preserves destination");
    Check(!F::Move(Name(root / "source").c_str(), Name(root / "destination").c_str()) && Load(root / "destination") == "sentinel", "no-replace move preserves destination");
    Check(F::Copy(Name(root / "source").c_str(), Name(root / "destination").c_str(), true) && Load(root / "destination") == "source", "explicit overwrite copy");
    Check(F::SetAttributes(Name(root / "source").c_str(), 1 | 0x20), "set read-only attribute");
    Check(OsGetFileAttributes(Name(root / "source").c_str()) & 1, "read-only attribute reported");
    Check(F::SetAttributes(Name(root / "source").c_str(), 0x20), "restore writable attribute");
    Check(F::SimpleGlob("Patch.MPQ", "p*.mpq") && F::SimpleGlob("a*b", "a\\*b") && !F::SimpleGlob("abc", "a?"), "legacy glob wildcard/escape/case behavior");
    char small[2] = {'!', '!'};
    Check(!F::MakeAbsolutePath(Name(root).c_str(), small, sizeof(small), true) && !small[0], "path output overflow is explicit and cleared");
    unsigned visits = 0;
    auto callback = [](const F::ProcessDirParms& parms) { ++*static_cast<unsigned*>(parms.m_cookie); return false; };
    Check(!F::ProcessDirFast(Name(root).c_str(), &visits, callback, true) && visits == 1, "directory callback can stop enumeration");
    Check(!F::RemoveDirectory(Name(root).c_str()), "nonempty directory removal fails");
}
void Interrupted(const fs::path& root) {
#if defined(WOWDECOMP_FILES_FAULT_FIXTURES)
    const auto path = root / "interrupted.bin";
    Save(path, "abcdef"); File file(path, 3 | 12 | 0x1000);
    char data[8]{}; int32_t count = 6;
    SetReadFault(1);
    Check(F::Read(file.value, data, &count) && count == 6 && !std::memcmp(data, "abcdef", 6), "interrupted pread retried through production provider");
    Check(F::SetPos(file.value, 0, 0), "reset interrupted fixture");
    count = 6; SetReadFault(2);
    Check(F::Read(file.value, data, &count) && count == 2 && Position(file.value) == 2, "short syscall count and cursor preserved");
    count = 6; SetReadFault(3);
    Check(!F::Read(file.value, data, &count) && count == 0 && Position(file.value) == 2, "hard read error clears count without advancing cursor");
    count = 3; SetWriteFault(1);
    Check(F::Write(file.value, "XYZ", &count) && count == 3 && Position(file.value) == 5, "interrupted pwrite retried");
    count = 3; SetWriteFault(2);
    Check(F::Write(file.value, "XYZ", &count) && count == 2 && Position(file.value) == 7, "short write count preserved");
    count = 3; SetWriteFault(3);
    Check(!F::Write(file.value, "XYZ", &count) && count == 0 && Position(file.value) == 7, "hard write error preserves cursor");
#else
    (void)root;
    std::puts("SKIP: syscall interruption/short-transfer injection is Linux-only");
#endif
}
}
int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    StormRtlInitialize();
    Directory directory;
    std::puts("Check: creation dispositions and unsupported flags");
    Creation(directory.path);
    std::puts("Check: positioned/partial I/O and sparse 64-bit offsets");
    Transfers(directory.path);
    std::puts("Check: concurrent reads, thread-local errors and no-replace moves");
    Concurrent(directory.path);
    std::puts("Check: data/settings/cache roots and path aliases");
    Paths(directory.path);
    std::puts("Check: file metadata, copy, enumeration and nonregular files");
    FileOperations(directory.path);
    std::puts("Check: interrupted/short/hard I/O outcomes");
    Interrupted(directory.path);
    std::puts("PASS: native file, path-root and settings fixtures (synthetic data only)");
}
