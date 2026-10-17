// Fixture preparation only: do not allocate/zero several GiB on the runner.
// The subsequent resize/write/read still use the production native provider.
#include "Engine/Source/Storm/H/storm.h"
#include <filesystem>
#include <windows.h>
#include <winioctl.h>

bool PrepareSparseFixture(const std::filesystem::path& path) {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                             nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) { SErrSetLastError(GetLastError()); return false; }
    DWORD returned = 0;
    const BOOL result = DeviceIoControl(file, FSCTL_SET_SPARSE, nullptr, 0, nullptr, 0, &returned, nullptr);
    const DWORD error = GetLastError();
    CloseHandle(file);
    if (!result) SErrSetLastError(error);
    return result != 0;
}
