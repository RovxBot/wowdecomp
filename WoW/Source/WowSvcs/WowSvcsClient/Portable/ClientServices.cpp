// Selected original ClientServices/ClientConnection lifecycle (12340
// 0x006B0250, 0x006B0810, 0x006B10B0, 0x006B1390, 0x006B14C0,
// 0x006B1E00, 0x006B1F80, 0x006B2000, 0x006B2CF0, 0x006B2E30).
// TEST-SERVER only: Event owns transport; Glue owns these objects. No new
// VERIFY claim, Warden approval, gameplay owner or fabricated server callback.
#include "WoW/Source/WowSvcs/WowSvcsClient/ClientServices.h"
#include "WoW/Source/WowServices/Login.h"
#include "WoW/Source/WowServices/Portable/TestServerLogin.h"
#include "WoW/Source/WowServices/Portable/Session.h"
#include "Engine/Source/Storm/H/storm.h"
#include <cstring>
#include <stdexcept>

ClientServices* ClientServices::m_instance;
RealmResponse* ClientServices::s_clientRealmResponse;
Login* ClientServices::s_loginObj;
ClientConnection* ClientServices::s_currentConnection;
ClientConnection* g_clientConnection;
char ClientServices::s_accountName[1280];
REALM_INFO ClientServices::m_selectRealmInfo;
bool ClientServices::m_selectRealmInfoValid;
namespace {
[[noreturn]] void Unsupported() {
    throw std::logic_error("Unsupported operation in native character-selection profile");
}
}
ClientConnection::ClientConnection(RealmResponse* response) : RealmConnection(response),
    m_connected(0), m_characterLoggedIn(0), m_statusComplete(1), m_statusResult(1),
    m_statusOp(COP_NONE), m_errorCode(RESPONSE_SUCCESS), m_inGame(0),
    m_quitAfterLogout(0), m_loggingOut(0), m_realmConnectPending(0), m_cleanup(nullptr) {}
ClientConnection::~ClientConnection() = default;
void ClientConnection::Initiate(WOWCS_OPS op, int32_t error, void (*cleanup)()) {
    NativeSession::CheckOwner();
    m_cleanup = cleanup; m_statusOp = op; m_errorCode = error; m_statusComplete = 0;
}
void ClientConnection::Complete(int32_t result, int32_t error) {
    if (m_cleanup) { auto cleanup = m_cleanup; m_cleanup = nullptr; cleanup(); }
    m_statusResult = result; m_errorCode = error; m_statusComplete = 1;
}
void ClientConnection::Cancel(int32_t error) { Complete(0, error); }
void ClientConnection::Cleanup() {
    if (m_cleanup) { auto cleanup = m_cleanup; m_cleanup = nullptr; cleanup(); }
}
int32_t ClientConnection::HandleConnect() {
    m_connected = 1;
    Initiate(COP_AUTHENTICATE, CSTATUS_AUTHENTICATING, nullptr);
    return RealmConnection::HandleConnect();
}
int32_t ClientConnection::HandleDisconnect() {
    m_connected = 0; m_loggingOut = 0;
    // Preserve a concrete server rejection across its following close.
    if (!m_statusComplete || m_statusResult) Complete(0, RESPONSE_DISCONNECTED);
    return RealmConnection::HandleDisconnect();
}
int32_t ClientConnection::HandleCantConnect() {
    m_connected = 0; Complete(0, RESPONSE_FAILED_TO_CONNECT);
    return RealmConnection::HandleCantConnect();
}
void ClientConnection::Disconnect() {
    NativeSession::CheckOwner();
    m_connected = m_authenticated = 0;
    m_nativeAuthSent = m_nativeEnumPending = false;
    m_characterList.Clear();
    NetClient::Disconnect();
}
void ClientConnection::Connect() {
    if (!m_statusComplete || !ClientServices::s_loginObj || !ClientServices::s_loginObj->IsLoggedOn() ||
        ClientServices::s_loginObj->m_logonStatus != 0)
        throw std::logic_error("Realm connection requires verified live login");
    m_realmConnectPending = 0;
    Initiate(COP_CONNECT, CSTATUS_CONNECTING, nullptr);
    ClientServices::ConnectToSelectedServer();
}
void ClientConnection::HandleAuthResult(int32_t result) { Complete(result == AUTH_OK, result); }
void ClientConnection::GetCharacterList() {
    Initiate(COP_GET_CHARACTERS, CHAR_LIST_RETRIEVING, nullptr);
    if (m_connected && m_authenticated) RequestCharacterEnum();
    else Cancel(RESPONSE_FAILED_TO_CONNECT);
}
int32_t ClientConnection::EnumerateCharacters(void (*callback)(CHARACTER_INFO&, void*), void* param) {
    if (!callback) throw std::invalid_argument("Missing character enumeration consumer");
    for (uint32_t i = 0; i < m_characterList.Count(); ++i) callback(m_characterList[i], param);
    return m_characterList.Count();
}
void ClientConnection::HandleCharacterCreate(uint8_t) { Unsupported(); }
void ClientConnection::HandleCharacterDelete(uint8_t) { Unsupported(); }
void ClientConnection::HandleCharacterLoginFailed(uint8_t) { Unsupported(); }
void ClientConnection::HandleLogoutComplete() { Unsupported(); }
void ClientConnection::HandleLogoutResponse(int32_t, int32_t) { Unsupported(); }
void ClientConnection::HandleLogoutAbortAck() { Unsupported(); }

ClientConnection* ClientServices::Connection() { NativeSession::CheckOwner(); return s_currentConnection; }
Login* ClientServices::LoginConnection() { return s_loginObj; }
LoginResponse* ClientServices::CreateLoginResponse() {
    if (!m_instance) throw std::logic_error("Native ClientServices owner is closed");
    return m_instance;
}
bool ClientServices::GetCdKeys(Grunt::CdKey&) { return false; }
void ClientServices::FileData(const uint8_t*, uint32_t) { Unsupported(); }
const char* ClientServices::GetLoginServer() { Unsupported(); }
int32_t ClientServices::GetLoginServerType() { return 0; }
void ClientServices::JoinRealmResult(uint32_t, int32_t, int32_t, int32_t, int32_t) { Unsupported(); }
void ClientServices::LoginServerStatus(LOGIN_STATE state, LOGIN_RESULT, const char*, const char*, const char*, uint16_t) {
    NativeSession::CheckOwner();
    if (state != LOGIN_STATE_AUTHENTICATED) return;
    if (!s_loginObj || !s_loginObj->IsLoggedOn() || s_loginObj->m_logonStatus != 0 || !s_currentConnection)
        throw std::logic_error("Session handoff requires verified SRP and live connection owner");
    LoginData data{};
    SStrCopy(data.m_account, s_loginObj->m_username, sizeof(data.m_account));
    data.m_loginServerID = s_loginObj->GetServerId();
    data.m_loginServerType = s_loginObj->GetLoginServerType();
    std::memcpy(data.m_sessionKey, s_loginObj->m_sessionKey, sizeof(data.m_sessionKey));
    s_currentConnection->SetLoginData(&data);
    SStrCopy(s_accountName, data.m_account, sizeof(s_accountName));
    volatile uint8_t* bytes = reinterpret_cast<volatile uint8_t*>(&data);
    for (size_t i = 0; i < sizeof(data); ++i) bytes[i] = 0;
}
void ClientServices::RealmEnumCallback(uint32_t result) {
    if (!s_currentConnection) throw std::logic_error("Realm response has no native owner");
    if (s_currentConnection->m_statusOp == COP_GET_REALMS || s_currentConnection->m_statusOp == COP_NONE)
        s_currentConnection->Complete(result == 0, result == 0 ? REALM_LIST_SUCCESS : REALM_LIST_FAILED);
}
REALM_INFO* ClientServices::GetRealmInfoByIndex(uint32_t index) {
    auto* response = CreateLoginResponse();
    return index < response->m_realmList.Count() ? &response->m_realmList[index] : nullptr;
}
REALM_INFO* ClientServices::GetSelectedRealm() { return m_selectRealmInfoValid ? &m_selectRealmInfo : nullptr; }
const char* ClientServices::GetSelectedRealmName() {
    auto* realm = GetSelectedRealm(); return realm ? realm->m_realmName : "";
}
bool ClientServices::SelectRealm(const char* name) {
    NativeSession::CheckOwner();
    m_selectRealmInfoValid = false;
    if (!name) return false;
    auto* response = CreateLoginResponse();
    for (uint32_t i = 0; i < response->m_realmList.Count(); ++i) {
        auto& realm = response->m_realmList[i];
        if (!SStrCmpUTF8I(name, realm.m_realmName, STORM_MAX_STR)) {
            if (realm.m_flags & 3 || realm.m_otherFlags & 1 ||
                ((realm.m_flags & 4) && realm.m_versionBuild != 12340)) return false;
            NativeTestServer::ValidateEndpoint(realm.m_realmAddress);
            m_selectRealmInfo = realm;
            m_selectRealmInfoValid = true;
            return true;
        }
    }
    return false;
}
void ClientServices::ConnectToSelectedServer() {
    NativeSession::CheckOwner();
    if (!m_selectRealmInfoValid || !s_currentConnection || !s_loginObj || !s_loginObj->IsLoggedOn() || s_loginObj->m_logonStatus != 0)
        throw std::logic_error("No authenticated advertised realm selected");
    auto& realm = m_selectRealmInfo;
    NativeTestServer::ValidateEndpoint(realm.m_realmAddress);
    s_currentConnection->SetSelectedRealm(realm.m_regionId, realm.m_battlegroupId, realm.m_realmId);
    s_currentConnection->NetClient::Connect(realm.m_realmAddress);
}
void ClientServices::GetRealmList() {
    if (!s_loginObj || !s_loginObj->IsLoggedOn() || s_loginObj->m_logonStatus != 0)
        throw std::logic_error("Login session expired; sign in again");
    s_currentConnection->Initiate(COP_GET_REALMS, REALM_LIST_IN_PROGRESS, nullptr);
    s_loginObj->GetRealmList();
}
void ClientRealmResponseAdapter::HandleAuthResponse(RealmConnection* connection, uint8_t result) {
    auto* client = static_cast<ClientConnection*>(connection);
    if (result == AUTH_WAIT_QUEUE) client->Initiate(COP_WAIT_QUEUE, AUTH_WAIT_QUEUE, nullptr);
    else client->HandleAuthResult(result);
}
void ClientRealmResponseAdapter::CharacterListReceived(RealmConnection* connection, const TSFixedArray<CHARACTER_INFO>&, int32_t success) {
    static_cast<ClientConnection*>(connection)->Complete(success, success ? CHAR_LIST_RETRIEVED : CHAR_LIST_FAILED);
}
void ClientRealmResponseAdapter::GameServerResult(RealmConnection*, const char*, const char*, const char*) {
    // Original informational trace callback; never log accounts/session contents.
}
void ClientRealmResponseAdapter::HandleConnect(RealmConnection*) {}
void ClientRealmResponseAdapter::HandleDisconnect(RealmConnection*) {}
void ClientRealmResponseAdapter::HandleCantConnect(RealmConnection*) {}
void ClientRealmResponseAdapter::AddOnListReceived(RealmConnection*) {}
void ClientRealmResponseAdapter::LoginServerResult(LOGIN_STATE, LOGIN_RESULT, const char*, const char*, const char*) { Unsupported(); }
void ClientRealmResponseAdapter::RealmListReceived(TSFixedArray<REALM_INFO>&) { Unsupported(); }
void ClientRealmResponseAdapter::MessageReceived(RealmConnection*, uint32_t) { Unsupported(); }
void ClientRealmResponseAdapter::OnChatReceived(RealmConnection*, WOWGUID, const char*, uint8_t) { Unsupported(); }
void ClientRealmResponseAdapter::OnAccountOnline(RealmConnection*, int32_t, WOWGUID) { Unsupported(); }
void ClientRealmResponseAdapter::OnAFKMonitorInfoResponse(RealmConnection*, WOWGUID, int32_t, int32_t, int32_t, int32_t) { Unsupported(); }
