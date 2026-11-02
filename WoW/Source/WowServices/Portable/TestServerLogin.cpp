// Native test-only adaptation of original GruntLogin::GetLogonMethod (12340
// 0x008CA580). No Windows body/provenance or production integrity claim changes.
#include "TestServerLogin.h"
#include "WoW/Source/WowServices/LoginResponse.h"
#include "WoW/Source/WowServices/Portable/Session.h"
#include "Engine/Source/Os/Time.h"
#include "Engine/Source/Storm/H/storm.h"
#include "GameVersion.h"
#include <cstring>
#include <memory>
#include <stdexcept>

namespace {
void Erase(char* password) {
    if (!password) return;
    volatile char* byte = password;
    while (*byte) *byte++ = 0;
    SMemFree(password, __FILE__, __LINE__, 0);
}
}
namespace NativeTestServer {
void ValidateEndpoint(const char* endpoint) {
    // No implicit retail destination, URI, whitespace or silent truncation.
    if (!endpoint || !*endpoint || std::strlen(endpoint) > 255)
        throw std::invalid_argument("Test-server login requires an explicit external realmlist host[:port]");
    const char* colon = nullptr;
    for (const char* p = endpoint; *p; ++p) {
        if (*p == ':' && !colon) { colon = p; continue; }
        if (!( (*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
               (*p >= '0' && *p <= '9') || *p == '.' || *p == '-'))
            throw std::invalid_argument("Invalid test-server realmlist host[:port]");
    }
    if (colon) {
        unsigned port = 0;
        if (colon == endpoint || !colon[1]) throw std::invalid_argument("Invalid test-server port");
        for (const char* p = colon + 1; *p; ++p) {
            if (*p < '0' || *p > '9' || port > 6553) throw std::invalid_argument("Invalid test-server port");
            port = port * 10 + unsigned(*p - '0');
        }
        if (!port || port > 65535) throw std::invalid_argument("Invalid test-server port");
    }
}
Login::Login(uint32_t value) : locale(value) { NativeSession::CheckOwner(); }
void Login::Logon(const char* endpoint, const char* portal) {
    NativeSession::CheckOwner();
    ValidateEndpoint(endpoint);
    GruntLogin::Logon(endpoint, portal);
}
Login::~Login() {
    Erase(m_password);
    m_password = nullptr;
    volatile uint8_t* key = m_sessionKey;
    for (size_t i = 0; i < sizeof(m_sessionKey); ++i) key[i] = 0;
}
void Login::GetLogonMethod() {
    NativeSession::CheckOwner();
    if (!m_password || IsReconnect()) return;
    // Retire the credential even if a callback deletes this owner or throws.
    std::unique_ptr<char, decltype(&Erase)> password(m_password, Erase);
    m_password = nullptr;
    const auto alive = m_clientLink->m_nativeAlive;
    Grunt::ClientLink::Logon identity{};
    identity.m_programId = 0x00576f57; // 'WoW', retail unchanged 12340 data
    identity.m_processor = X64Identity;
    identity.m_os = LinuxIdentity;
    identity.m_versionMajor = GAME_VERSION_MAJOR;
    identity.m_versionMinor = GAME_VERSION_MINOR;
    identity.m_versionPatch = GAME_VERSION_PATCH;
    identity.m_build = GAME_BUILD;
    identity.m_locale = locale;
    identity.m_timeZone = OsGetTimeZone();
    identity.m_accountName = m_username;
    identity.m_password = password.get();
    m_loginResponse->UpdateLoginStatus(LOGIN_STATE_AUTHENTICATING, LOGIN_OK, nullptr, 0);
    if (*alive) m_clientLink->LogonNewSession(identity);
}
void Login::ProveUnavailableVersion() {
    NativeSession::CheckOwner();
    if (m_pinEnabled || m_matrixEnabled || m_tokenEnabled || IsReconnect())
        throw std::logic_error("Test profile does not support PIN/matrix/token/reconnect");
    const uint8_t unavailable[20] = {};
    ProveVersion(unavailable);
}
}
