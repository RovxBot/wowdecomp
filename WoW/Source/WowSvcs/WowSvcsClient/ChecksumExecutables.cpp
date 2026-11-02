#include "ChecksumExecutables.h"
#include <cstring>
#include "Engine/Source/BlizzardCore/Source/Packages/Util/SHA1.h"
#include "Engine/Source/Os/OsVersionHash.h"
#include "WoW/Source/WowServices/HMAC.h"
#if defined(WOWDECOMP_NATIVE_CONNECTION)
#include <stdexcept>
#else

struct ChecksumFile {
    /* 0000 */ const char* m_path;
    /* 0004 */ int32_t m_type;
};

// STATIC: 0x00AD96E8
static ChecksumFile s_files[3] = { { nullptr, 2 }, { "DivxDecoder.dll", 2 }, { "unicows.dll", 1 } };
#endif

// VERIFY: b726580e manual
// OFFSET: 0x006B9CF0
void ChecksumExecutables(const uint8_t* challenge, uint32_t challengeLength, uint8_t* checksum) {
#if defined(WOWDECOMP_NATIVE_CONNECTION)
    // Native adaptation, not covered by the Windows VERIFY annotation above.
    // The original inventory requires loaded PE images. File-mode support alone
    // cannot qualify that inventory or a Linux/Mach-O executable version proof.
    // The void legacy signature cannot report failure: reject before publishing
    // an empty/partial HMAC, reading challenge bytes or modifying the output.
    throw std::runtime_error("Native executable version-checksum inventory is unsupported");
#else
    HMAC<SHA1> hmac;
    hmac.Init(challenge, challengeLength);

    for (uint32_t i = 0; i < sizeof(s_files) / sizeof(s_files[0]); i++) {
        if (s_files[i].m_type) {
            IOsVersionHashCallback2<HMAC<SHA1>> callback(&hmac);
            IOsVersionChecksum(callback, s_files[i].m_path, s_files[i].m_type == 2);
        }
    }

    SHA1::Digest digest;
    hmac.Finish(digest);
    memcpy(checksum, &digest, sizeof(digest));
#endif
}
