// Native adaptation of the file-mode IOsVersionChecksum in Win64/OsVersionHash.cpp
// (12340 0x00871F90). No Windows loaded-image normalization or verification claim.
#include "Engine/Source/Os/OsVersionHash.h"
#include "Engine/Source/Os/OsFile.h"
#include <new>
#include <vector>

int32_t IOsVersionChecksum(IOsVersionHashCallback& callback, const char* path, int32_t loaded) {
    // ELF/Mach-O loaded images have no qualified 12340 version-hash contract.
    // Never reinterpret a native executable or a reference PE as that identity.
    if (loaded || !path || !*path) return 0;

    struct File {
        HOSFILE value;
        ~File() { if (value != OS_INVALID_FILE) OsCloseFile(value); }
    } file{OsCreateFile(path, OS_GENERIC_READ, 1, OS_OPEN_EXISTING,
                        OS_FILE_ATTRIBUTE_NORMAL, OS_FILE_TYPE_ANY)};
    if (file.value == OS_INVALID_FILE) return 0;
    const uint64_t size = OsGetFileSize(file.value);
    if (size > UINT32_MAX) return 0;

    // Retain the original single callback, after a complete successful read.
    // An I/O/allocation failure must not feed a prefix into the caller's HMAC.
    std::vector<uint8_t> bytes;
    try { bytes.resize(size ? static_cast<size_t>(size) : 1); }
    catch (const std::bad_alloc&) { return 0; }
    uint32_t count = 0;
    if (!OsReadFile(file.value, bytes.data(), static_cast<uint32_t>(size), &count) ||
        count != size || OsGetFileSize(file.value) != size) return 0;
    uint8_t tail;
    if (!OsReadFile(file.value, &tail, 1, &count) || count) return 0;
    const bool closed = OsCloseFile(file.value);
    file.value = OS_INVALID_FILE;
    if (!closed) return 0;
    callback.Process(bytes.data(), static_cast<uint32_t>(size));
    return 1;
}
