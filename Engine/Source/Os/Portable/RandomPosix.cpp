#include "Engine/Source/Os/OsSecureRandom.h"
#include "Engine/Source/Storm/H/storm.h"
#include <cerrno>
#include <unistd.h>
#if defined(__linux__)
#include <sys/random.h>
#endif

void OsSecureRandom(uint8_t* buffer, uint32_t size) {
    if (!buffer && size) SErrDisplayAppFatalMessage("Null random buffer", __FILE__, __LINE__);
    while (size) {
#if defined(__linux__)
        ssize_t result = getrandom(buffer, size, 0);
        if (result < 0 && errno == EINTR) continue;
        if (result <= 0) SErrDisplayAppFatalMessage("getrandom failed", __FILE__, __LINE__);
        buffer += result;
        size -= static_cast<uint32_t>(result);
#else
        uint32_t count = size > 256 ? 256 : size;
        if (getentropy(buffer, count)) SErrDisplayAppFatalMessage("getentropy failed", __FILE__, __LINE__);
        buffer += count;
        size -= count;
#endif
    }
}
void IOsSecureRandom(uint8_t* buffer, uint32_t size) { OsSecureRandom(buffer, size); }
