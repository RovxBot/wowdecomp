#include "Engine/Source/Os/OsSecureRandom.h"
#include "Engine/Source/Storm/H/storm.h"
#include <windows.h>
#include <bcrypt.h>

void OsSecureRandom(uint8_t* buffer, uint32_t size) {
    if (!size) return;
    if (!buffer || BCryptGenRandom(nullptr, buffer, size, BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0)
        SErrDisplayAppFatalMessage("BCryptGenRandom failed", __FILE__, __LINE__);
}
void IOsSecureRandom(uint8_t* buffer, uint32_t size) { OsSecureRandom(buffer, size); }
