#ifndef WOWDECOMP_NATIVE_TEST_SERVER_LOGIN_H
#define WOWDECOMP_NATIVE_TEST_SERVER_LOGIN_H
#if !defined(WOWDECOMP_NATIVE_TEST_SERVER_LOGIN) || !defined(__linux__) || !defined(__x86_64__)
#error Test-server login is available only in explicitly enabled Linux x86-64 test builds
#endif
#include "WoW/Source/WowServices/GruntLogin.h"

namespace NativeTestServer {
// Nonstandard, truthful Grunt fourCCs. Serialized little endian by PackLogon,
// just like the original Win/OSX identifiers. Server support is an external requirement.
constexpr uint32_t LinuxIdentity = 0x004c6e78; // 'Lnx'
constexpr uint32_t X64Identity = 0x00783634;   // 'x64'
void ValidateEndpoint(const char* endpoint);
class Login final : public GruntLogin {
public:
    explicit Login(uint32_t locale);
    ~Login() override;
    void Logon(const char* endpoint, const char* portal) override;
    void GetLogonMethod() override;
    // The required wire field is SHA1(A || 20 zero bytes). This is an explicit
    // unavailable-checksum placeholder, never an executable integrity result.
    void ProveUnavailableVersion();
private:
    uint32_t locale;
};
}
#endif
