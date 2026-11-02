// Selected original 12340 GlueScriptEvents_Scripts.cpp services. Native-only
// adaptation: retained CVar/FrameScript owners, mounted local packages and Event
// shutdown. Original world GetBuildInfo is a separate four-result API.
#include "ScriptServices.h"
#include "WoW/Source/Glue/GlueMgr.h"
#include "WoW/Source/Glue/CGlueMgr.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/FrameScript/FrameScript_Object.h"
#include "Engine/Source/FrameScript/Portable/Runtime.h"
#include "Engine/Source/FrameXML/LoadXML.h"
#include "Engine/Source/ConsoleBase/ConsoleVar.h"
#include "Engine/Source/ConsoleBase/Types.h"
#include "Engine/Source/Base/Coordinate.h"
#include "Engine/Source/BlizzardCore/SFile2-Core.h"
#include "Engine/Source/ShaderEffect/ShadowCache.h"
#include "Engine/Source/Storm/H/storm.h"
#include "Engine/Source/Event/Event.h"
#include "Engine/Source/Os/Processor.h"
#include "Engine/Source/Services/AsyncFile.h"
#include "Engine/Source/Os/TimeManager.h"
#include "GameVersion.h"
#include <cstdio>
#include <cstring>
#include <deque>
#include <string>
#include <stdexcept>
#if defined(WOWDECOMP_NATIVE_TEST_SERVER_LOGIN)
#include "WoW/Source/WowServices/Portable/TestServerLogin.h"
#include "WoW/Source/WowServices/LoginResponse.h"
#include "WoW/Source/WowSvcs/WowSvcsClient/ClientServices.h"
#include "WoW/Source/Glue/RealmList.h"
#include "WoW/Source/Glue/CharacterSelection.h"
#include "WoW/Source/Glue/Portable/RealmData.h"
#include <memory>
#endif
// Native selection of original CGlueMgr activation state. Warning text is owned
// until acknowledgement/close, rather than borrowing collectable Lua strings.
char CGlueMgr::m_currentScreen[64];
uint8_t CGlueMgr::m_authenticated;
uint32_t CGlueMgr::m_numChangedOptionWarnings;
TSGrowableArray<const char*> CGlueMgr::m_changedOptionWarnings;
// Original client agreement state; external Config.wtf remains the sole saved owner.
CVar* g_readTOSVar;
int32_t CGlueMgr::m_acceptedTOS;
int32_t CGlueMgr::m_rereadTOS;
CVar* g_readEULAVar;
int32_t CGlueMgr::m_acceptedEULA;
int32_t CGlueMgr::m_rereadEULA;
CVar* g_readTerminationWithoutNoticeVar;
int32_t CGlueMgr::m_acceptedTerminationWithoutNotice;
int32_t CGlueMgr::m_rereadTerminationWithoutNotice;
CVar* g_readScanningVar;
int32_t CGlueMgr::m_acceptedScanning;
int32_t CGlueMgr::m_rereadScanning;
CVar* g_readContestVar;
int32_t CGlueMgr::m_acceptedContest;
int32_t CGlueMgr::m_rereadContest;
namespace {
std::deque<std::string> warningText;
CVar* g_accountNameVar;
CVar* g_accountListVar;
CVar* g_accountUsesTokenVar;
bool registered = false;
uint8_t clientExpansionLevel = 0;
// ScanDLLGlue.cpp's Windows DLL download/signature/ordinal-call pipeline is
// unavailable on native Linux. Original AccountLogin's SCANDLL_ERROR handler
// calls ContinueAnyway before showing the controls. Permit that presentation
// transition while retaining the failed scan. Normal builds refuse every login.
enum class ScanState { NotStarted, ErrorPending, Unavailable };
ScanState scanState = ScanState::NotStarted;
bool scanDispatching = false;
bool unavailableDialog = false;
const char* pendingDialog = nullptr;
constexpr const char* scanUnavailable =
    "Sign-in is unavailable: native platform identity, executable version proof and integrity "
    "services are unsupported. Sign-in has not been approved.";
#if defined(WOWDECOMP_NATIVE_TEST_SERVER_LOGIN)
bool testServerProfile = false;
std::string testEndpoint;
constexpr const char* testCapabilities =
    "TEST SERVER ONLY: Linux x86-64 identity. ScanDLL, executable integrity and Warden "
    "are unavailable; no successful checks are claimed. Requires the isolated server's "
    "StrictVersionCheck=0 and Warden.Enabled=0. This profile targets character selection; world entry is unavailable.";
struct TestResponse final : ClientServices {
    LOGIN_STATE state = LOGIN_STATE_INITIALIZED;
    LOGIN_RESULT result = LOGIN_OK;
    std::string resultName;
    bool changed = false, realms = false;
    uint32_t realmResult = 0;
    const char* GetLoginServer() override { return testEndpoint.c_str(); }
    int32_t GetLoginServerType() override { return 0; }
    void JoinRealmResult(uint32_t, int32_t, int32_t, int32_t, int32_t) override {
        throw std::logic_error("Native test profile cannot join a world session");
    }
    void LoginServerStatus(LOGIN_STATE value, LOGIN_RESULT status, const char*, const char*,
                           const char* name, uint16_t) override {
        // A subsequent close can emit both DISCONNECTED and a generic FAILED.
        // Preserve the first server/proof error until outer idle presents it.
        if (state == LOGIN_STATE_FAILED) return;
        ClientServices::LoginServerStatus(value, status, nullptr, nullptr, name, 0);
        state = value; result = status; resultName = name; changed = true;
    }
    void RealmEnumCallback(uint32_t value) override {
        ClientServices::RealmEnumCallback(value);
        realms = true; realmResult = value;
    }
};
// Response outlives login. Network callbacks only record state; UI delivery and
// cancellation happen on outer idle, after the original password field clears.
TestResponse testResponse;
std::unique_ptr<NativeTestServer::Login> testLogin;
ClientRealmResponseAdapter testRealmResponse;
std::unique_ptr<ClientConnection> testWorld;
bool testStartPending = false, testCancelPending = false, testRealmRequested = false;
bool testRealmsReady = false, testConnectPending = false, testWorldStarted = false;
bool testCharacterRequestPending = false;
bool testRealmRequestPending = false;
bool testPublishingCharacters = false;
uint32_t testQueuePosition = UINT32_MAX;
uint32_t testWorldDeadline = 0;
bool testDialog = false;
void ResetTestLogin() {
    GlueNative::CloseCharacterServices();
    CGlueMgr::m_authenticated = 0;
    ClientServices::m_selectRealmInfoValid = false;
    ClientServices::s_currentConnection = nullptr;
    ClientServices::s_loginObj = nullptr;
    ClientServices::m_instance = nullptr;
    std::memset(ClientServices::s_accountName, 0, sizeof(ClientServices::s_accountName));
    testWorld.reset();
    testLogin.reset();
    CRealmList::Shutdown();
    testResponse.m_realmList.Clear();
    testResponse.state = LOGIN_STATE_INITIALIZED;
    testResponse.result = LOGIN_OK;
    testResponse.resultName.clear();
    testResponse.changed = testResponse.realms = false;
    testStartPending = testCancelPending = testRealmRequested = false;
    testRealmsReady = testConnectPending = testWorldStarted = testCharacterRequestPending = false;
    testRealmRequestPending = false;
    testPublishingCharacters = false; testQueuePosition = UINT32_MAX;
}
void TestDialog(const char* kind, const char* message) {
    testDialog = true;
    FrameScript_SignalEvent(GLUE_EVENT_OPEN_STATUS_DIALOG, "%s%s", kind, message);
}
void RetireTestSession() {
    const bool returnToLogin = !SStrCmpI(CGlueMgr::m_currentScreen,"charselect",STORM_MAX_STR);
    ResetTestLogin();
    if (returnToLogin) FrameScript_SignalEvent(GLUE_EVENT_SET_GLUE_SCREEN,"%s","login");
}
void PollTestLogin() {
    if (testCancelPending) {
        RetireTestSession(); testDialog = false;
        FrameScript_SignalEvent(GLUE_EVENT_CLOSE_STATUS_DIALOG, nullptr);
        return;
    }
    if (!testLogin) return;
    if (testStartPending) {
        testStartPending = false;
        testLogin->Logon(testEndpoint.c_str(), nullptr);
    }
    if (testRealmRequestPending) {
        testRealmRequestPending = false;
        ClientServices::GetRealmList();
    }
    if (testResponse.realms) {
        const bool valid = !testResponse.realmResult;
        const auto count = testResponse.m_realmList.Count();
        testResponse.realms = false;
        if (!valid) {
            RetireTestSession(); TestDialog("OKAY", "TEST SERVER: realm list failed. No world session was started."); return;
        }
        testRealmsReady = true;
        std::printf("native-test-server=srp-verified advertised-realms=%u integrity=unavailable\n", count);
        if (!CRealmList::m_categories.Count()) CRealmList::Initialize();
        testDialog = false;
        FrameScript_SignalEvent(GLUE_EVENT_CLOSE_STATUS_DIALOG, nullptr);
        CRealmList::UpdateList();
        return;
    }
    if (testConnectPending) {
        testConnectPending = false;
        if (testWorldStarted) {
            testWorld = std::make_unique<ClientConnection>(&testRealmResponse);
            ClientServices::s_currentConnection = testWorld.get();
            ClientServices::m_instance->ClientServices::LoginServerStatus(LOGIN_STATE_AUTHENTICATED, LOGIN_OK, nullptr, nullptr, nullptr, 0);
        }
        testWorld->Connect(); testWorldStarted = true;
        testWorldDeadline = OsGetAsyncTimeMs() + 30000;
        TestDialog("CANCEL", "TEST SERVER: connecting to the selected advertised realm. Executable integrity and Warden remain unavailable.");
    }
    if (testWorldStarted) {
        if (testWorld->m_statusComplete && !testWorld->m_statusResult) {
            const int error = testWorld->m_errorCode;
            RetireTestSession();
            TestDialog("OKAY", ("TEST SERVER: world connection failed (server/status code " + std::to_string(error) + ").").c_str());
            return;
        }
        if (!testWorld->m_statusComplete && int32_t(OsGetAsyncTimeMs() - testWorldDeadline) >= 0) {
            RetireTestSession(); TestDialog("OKAY", "TEST SERVER: world connection/character enumeration timed out."); return;
        }
        if (testWorld->m_statusOp == COP_WAIT_QUEUE) {
            if (testQueuePosition != testWorld->m_queuePosition) {
                testQueuePosition = testWorld->m_queuePosition;
                testWorldDeadline = OsGetAsyncTimeMs() + 120000;
                TestDialog("CANCEL", ("TEST SERVER: server queue position " + std::to_string(testWorld->m_queuePosition)).c_str());
            }
        } else if (testWorld->m_statusOp == COP_AUTHENTICATE && testWorld->m_statusComplete && testWorld->m_authenticated) {
            std::printf("native-test-server=world-authenticated integrity=unavailable warden=unavailable\n");
            testWorld->GetCharacterList();
            testWorldDeadline = OsGetAsyncTimeMs() + 30000;
            TestDialog("CANCEL", "TEST SERVER: world authentication accepted by server; retrieving characters.");
        } else if (testWorld->m_statusOp == COP_GET_CHARACTERS && testWorld->m_statusComplete) {
            testWorld->m_statusOp = COP_NONE;
            testDialog = false;
            FrameScript_SignalEvent(GLUE_EVENT_CLOSE_STATUS_DIALOG, nullptr);
            std::printf("native-test-server=characters-received count=%u\n", testWorld->m_characterList.Count());
            GlueNative::LoadCharacterServices();
            struct Publish { Publish() { testPublishingCharacters=true; } ~Publish() { testPublishingCharacters=false; } } publish;
            FrameScript_SignalEvent(GLUE_EVENT_SET_GLUE_SCREEN, "%s", "charselect");
            CCharacterSelection::UpdateCharacterList();
        }
        if (testCharacterRequestPending) {
            testCharacterRequestPending = false;
            if (testWorld->m_statusOp == COP_NONE) {
                testWorld->GetCharacterList(); testWorldDeadline = OsGetAsyncTimeMs() + 30000;
                TestDialog("CANCEL", "TEST SERVER: retrieving characters from the authenticated realm.");
            }
        }
    }
    if (!testResponse.changed) return;
    testResponse.changed = false;
    switch (testResponse.state) {
    case LOGIN_STATE_CHECKINGVERSIONS:
        testLogin->ProveUnavailableVersion();
        TestDialog("CANCEL", "TEST SERVER: executable checksum unavailable; sending the documented placeholder. Verifying SRP server proof.");
        return;
    case LOGIN_STATE_AUTHENTICATED:
        CGlueMgr::m_authenticated = 1;
        if (!testRealmRequested) {
            testRealmRequested = true; testLogin->GetRealmList();
            TestDialog("CANCEL", "TEST SERVER: SRP server proof verified. Retrieving advertised realms.");
        }
        return;
    case LOGIN_STATE_PIN: case LOGIN_STATE_MATRIX: case LOGIN_STATE_TOKEN:
    case LOGIN_STATE_DOWNLOADFILE: case LOGIN_STATE_SURVEY:
        RetireTestSession();
        TestDialog("OKAY", "TEST SERVER: requested security challenge or download is unsupported. Sign-in stopped.");
        return;
    case LOGIN_STATE_DISCONNECTED:
        if (testRealmsReady && testLogin->m_logonStatus == 0 && testResponse.result == LOGIN_OK)
            return; // Auth EOF does not invalidate the already handed-off verified key.
        [[fallthrough]];
    case LOGIN_STATE_FAILED: {
        const std::string name = testResponse.resultName;
        RetireTestSession();
        TestDialog("OKAY", ("TEST SERVER: sign-in failed (" + name + "). No world session was started.").c_str());
        return;
    }
    default:
        TestDialog("CANCEL", "TEST SERVER: connecting/authenticating with Linux identity. Integrity services are unavailable.");
    }
}
#endif
const char* ScanNotice() {
#if defined(WOWDECOMP_NATIVE_TEST_SERVER_LOGIN)
    if (testServerProfile) return testCapabilities;
#endif
    return scanUnavailable;
}
bool ServicesOpen() {
    return registered && CVar::m_initialized &&
        CVar::LookupRegistered("accountName") == g_accountNameVar &&
        CVar::LookupRegistered("accountList") == g_accountListVar &&
        CVar::LookupRegistered("g_accountUsesToken") == g_accountUsesTokenVar;
}
void RequireServices(lua_State* L) {
    FrameScript::Native::RequireOwner();
    if (!ServicesOpen())
        luaL_error(L, "Native Glue script services are closed");
}
void ShowUnavailable(const char* reason) {
    unavailableDialog = true;
    FrameScript_SignalEvent(GLUE_EVENT_OPEN_STATUS_DIALOG, "%s%s", "OKAY", reason);
}
// Native failure-only adaptations of original 12340 wrappers 0x004DCCF0,
// 0x004DCDF0 and 0x004DCE00. No verified Windows implementation is selected.
static int32_t Script_ScanDLLStart(lua_State* L) {
    RequireServices(L);
    if (!lua_isstring(L, 1) || !lua_isstring(L, 2))
        return luaL_error(L, "Usage: ScanDLLStart(\"VersionURL\", \"DLLURL\")");
    if (!scanDispatching) scanState = ScanState::ErrorPending;
    // Neither URL is retained or fetched; an empty non-Windows DLL URL is also
    // unsupported. Do not create files, load a library or start a scan worker.
    return 0;
}
static int32_t Script_ScanDLLContinueAnyway(lua_State* L) {
    RequireServices(L);
    // Unlike the Windows transition, this never sets m_scandllOkayToLogIn.
    // AccountLogin may display its controls; the integrity gate stays closed.
    return 0;
}
static int32_t Script_IsScanDLLFinished(lua_State* L) {
    RequireServices(L);
    lua_pushnil(L); // No successful native scan or approved continuation exists.
    return 1;
}
// Original API 0x004DC260 / CGlueMgr::LoginServerLogin 0x004D8A30 guards
// authentication separately from displaying the login UI. Normal builds refuse
// credential bytes; the separately compiled test profile never grants scan approval.
static int32_t Script_DefaultServerLogin(lua_State* L) {
    RequireServices(L);
    if (!lua_isstring(L, 1) || !lua_isstring(L, 2))
        return luaL_error(L, "Usage: DefaultServerLogin(\"accountName\", \"password\")");
#if defined(WOWDECOMP_NATIVE_TEST_SERVER_LOGIN)
    if (testServerProfile && CGlueMgr::m_acceptedTOS && CGlueMgr::m_acceptedEULA &&
        scanState == ScanState::Unavailable && !unavailableDialog && !scanDispatching) {
        if (testLogin || testDialog) return 0;
        size_t accountLength = 0, passwordLength = 0;
        const char* account = lua_tolstring(L, 1, &accountLength);
        const char* password = lua_tolstring(L, 2, &passwordLength);
        if (!accountLength || accountLength > 255 || !passwordLength || passwordLength > 255 ||
            std::memchr(account, 0, accountLength) || std::memchr(password, 0, passwordLength))
            return luaL_error(L, "Test-server credentials require nonempty bounded strings without embedded NULs");
        // Retail 12340/enUS is this integrated target's authenticated data locale.
        testLogin = std::make_unique<NativeTestServer::Login>(0x656e5553);
        testWorld = std::make_unique<ClientConnection>(&testRealmResponse);
        ClientServices::m_instance = &testResponse;
        ClientServices::s_loginObj = testLogin.get();
        ClientServices::s_currentConnection = testWorld.get();
        testLogin->Init(&testResponse);
        testLogin->SetLogonCreds(account, password);
        testStartPending = true;
        return 0;
    }
#endif
    // Input callbacks pin frame hierarchy. Deliver the notice on outer idle,
    // after the original login action has cleared its password edit field.
    pendingDialog = CGlueMgr::m_acceptedTOS && CGlueMgr::m_acceptedEULA
        ? ScanNotice() : "Sign-in is unavailable until the original agreements are accepted.";
    return 0;
}
// Original StatusDialogClick 0x004D98D0: acknowledge this owner's dialog or queue
// test-connection cancellation for outer idle, outside input callbacks.
static int32_t Script_StatusDialogClick(lua_State* L) {
    RequireServices(L);
#if defined(WOWDECOMP_NATIVE_TEST_SERVER_LOGIN)
    if (testDialog) { testCancelPending = true; return 0; }
#endif
    if (unavailableDialog) {
        unavailableDialog = false;
        FrameScript_SignalEvent(GLUE_EVENT_CLOSE_STATUS_DIALOG, nullptr);
    }
    return 0;
}
// OFFSET: 0x004DBE60
static int32_t Script_GetBuildInfo(lua_State* L) {
    RequireServices(L);
    lua_pushstring(L, FrameScript_GetText("VERSION", -1, GENDER_NOT_APPLICABLE));
    lua_pushstring(L, FrameScript_GetText("RELEASE_BUILD", -1, GENDER_NOT_APPLICABLE));
    lua_pushstring(L, GAME_VERSION);
    lua_pushstring(L, GAME_BUILD_STRING);
    lua_pushstring(L, GAME_BUILD_DATE);
    return 5;
}

// OFFSET: 0x004DBEC0
static int32_t Script_SetSavedAccountName(lua_State* L) {
    RequireServices(L);
    if (!lua_isstring(L, 1)) {
        luaL_error(L, "Usage: SetSavedAccountName(\"accountName\")");
        return 0;
    }

    g_accountNameVar->Set(lua_tolstring(L, 1, nullptr), true, false, false, true);
    return 0;
}

// OFFSET: 0x004DBF10
static int32_t Script_GetUsesToken(lua_State* L) {
    RequireServices(L);
    lua_pushboolean(L, g_accountUsesTokenVar->m_intValue);
    return 1;
}

// OFFSET: 0x004DBF30
static int32_t Script_SetUsesToken(lua_State* L) {
    RequireServices(L);
    if (lua_type(L, 1) != LUA_TBOOLEAN) {
        luaL_error(L, "Usage: SetUsesToken( 0 | 1 )");
        return 0;
    }

    g_accountUsesTokenVar->Set(lua_toboolean(L, 1), true, false, false, true);
    return 0;
}

// OFFSET: 0x004DBF80
static int32_t Script_SetSavedAccountList(lua_State* L) {
    RequireServices(L);
    if (!lua_isstring(L, 1)) {
        luaL_error(L, "Usage: SetSavedAccountList(\"accountList\")");
        return 0;
    }

    g_accountListVar->Set(lua_tolstring(L, 1, nullptr), true, false, false, true);
    return 0;
}

// OFFSET: 0x004DD5D0
static int32_t Script_GetSavedAccountName(lua_State* L) {
    RequireServices(L);
    lua_pushstring(L, g_accountNameVar->m_stringValue.m_str);
    return 1;
}

// OFFSET: 0x004DD5F0
static int32_t Script_GetSavedAccountList(lua_State* L) {
    RequireServices(L);
    lua_pushstring(L, g_accountListVar->m_stringValue.m_str);
    return 1;
}

// Original GetServerName (0x004DD900), bounded to the actual pre-authentication
// state: a saved realm name is a preference, with no selected live realm.
// Do not infer PvP/RP/availability or an endpoint from that name.
static int32_t Script_GetServerName(lua_State* L) {
    RequireServices(L);
#if defined(WOWDECOMP_NATIVE_TEST_SERVER_LOGIN)
    if (testServerProfile && ClientServices::GetSelectedRealm()) {
        auto* realm = ClientServices::GetSelectedRealm();
        int pvp=0, rp=0;
        for (int i=0;i<g_cfg_ConfigsDB.m_numRecords;++i) {
            const auto& config=g_cfg_ConfigsDB.m_records[i];
            if (config.m_realmType != realm->m_type) continue;
            pvp = config.m_playerKillingAllowed; rp = config.m_roleplaying; break;
        }
        lua_pushstring(L, realm->m_realmName);
        if (pvp) lua_pushnumber(L,1); else lua_pushnil(L);
        if (rp) lua_pushnumber(L,1); else lua_pushnil(L);
        if (realm->m_flags & 2) lua_pushnumber(L,1); else lua_pushnil(L);
        return 4;
    }
#endif
    if (CGlueMgr::m_authenticated
#if defined(WOWDECOMP_NATIVE_TEST_SERVER_LOGIN)
        && !testServerProfile
#endif
    )
        return luaL_error(L, "Native selected realm metadata unavailable");
    auto* name = CVar::LookupRegistered("realmName");
    if (!name) name = CVar::Register("realmName", "Last realm connected to", 0, "", nullptr, CATEGORY_NET, false, nullptr, false);
    lua_pushstring(L, name->m_stringValue.m_str);
    lua_pushnil(L); // No selected realm: original PvP/RP flags are unset.
    lua_pushnil(L);
    lua_pushnumber(L, 1.0); // Original no-realm 'down' result.
    return 4;
}
// Original IsTrialAccount's no-login fallback (0x004DD4D0). It reads an
// existing conversion preference, without inventing account entitlements.
static int32_t Script_IsTrialAccount(lua_State* L) {
    RequireServices(L);
#if defined(WOWDECOMP_NATIVE_TEST_SERVER_LOGIN)
    if (testServerProfile && testLogin && testLogin->m_logonStatus == 0) {
        if (testLogin->IsTrialAccount()) lua_pushnumber(L,1); else lua_pushnil(L);
        return 1;
    }
#endif
    if (CGlueMgr::m_authenticated)
        return luaL_error(L, "Native authenticated account metadata unavailable");
    auto* converted = CVar::Lookup("converted");
    if (converted && converted->m_intValue==0) lua_pushnumber(L,1.0);
    else lua_pushnil(L);
    return 1;
}
#if defined(WOWDECOMP_NATIVE_TEST_SERVER_LOGIN)
static int32_t Script_GetAccountExpansionLevel(lua_State* L) {
    RequireServices(L);
    if (!testServerProfile || !testWorld || !testWorld->m_authenticated)
        return luaL_error(L,"Account expansion requires actual world authentication");
    lua_pushnumber(L,testWorld->m_accountExpansion); return 1;
}
static int32_t Script_IsConnectedToServer(lua_State* L) {
    RequireServices(L);
    if (testServerProfile && testWorld && testWorld->m_connected) lua_pushnumber(L,1); else lua_pushnil(L);
    return 1;
}
static int32_t Script_DisconnectFromServer(lua_State* L) {
    RequireServices(L); testCancelPending = true; return 0;
}
static int32_t Script_EnterWorld(lua_State* L) {
    RequireServices(L);
    pendingDialog = "TEST SERVER: world entry is unavailable in this character-selection profile.";
    return 0;
}
#endif

// Original agreement query/explicit acceptance wrappers. Loading never accepts.
static int32_t Script_TOSAccepted(lua_State* L) {
    RequireServices(L);
    if (CGlueMgr::m_acceptedTOS) {
        lua_pushnumber(L, 1.0);
    } else {
        lua_pushnil(L);
    }

    return 1;
}
static int32_t Script_ShowTOSNotice(lua_State* L) {
    RequireServices(L);
    if (CGlueMgr::m_rereadTOS) {
        lua_pushnumber(L, 1.0);
    } else {
        lua_pushnil(L);
    }

    return 1;
}
static int32_t Script_AcceptTOS(lua_State* L) {
    RequireServices(L);
    CGlueMgr::AcceptTOS();
    return 0;
}
static int32_t Script_EULAAccepted(lua_State* L) {
    RequireServices(L);
    if (CGlueMgr::m_acceptedEULA) {
        lua_pushnumber(L, 1.0);
    } else {
        lua_pushnil(L);
    }

    return 1;
}
static int32_t Script_ShowEULANotice(lua_State* L) {
    RequireServices(L);
    if (CGlueMgr::m_rereadEULA) {
        lua_pushnumber(L, 1.0);
    } else {
        lua_pushnil(L);
    }

    return 1;
}
static int32_t Script_AcceptEULA(lua_State* L) {
    RequireServices(L);
    CGlueMgr::AcceptEULA();
    return 0;
}
static int32_t Script_TerminationWithoutNoticeAccepted(lua_State* L) {
    RequireServices(L);
    if (CGlueMgr::m_acceptedTerminationWithoutNotice) {
        lua_pushnumber(L, 1.0);
    } else {
        lua_pushnil(L);
    }

    return 1;
}
static int32_t Script_ShowTerminationWithoutNoticeNotice(lua_State* L) {
    RequireServices(L);
    if (CGlueMgr::m_rereadTerminationWithoutNotice) {
        lua_pushnumber(L, 1.0);
    } else {
        lua_pushnil(L);
    }

    return 1;
}
static int32_t Script_AcceptTerminationWithoutNotice(lua_State* L) {
    RequireServices(L);
    CGlueMgr::AcceptTerminationWithoutNotice();
    return 0;
}
static int32_t Script_ScanningAccepted(lua_State* L) {
    RequireServices(L);
    if (CGlueMgr::m_acceptedScanning) {
        lua_pushnumber(L, 1.0);
    } else {
        lua_pushnil(L);
    }

    return 1;
}
static int32_t Script_ShowScanningNotice(lua_State* L) {
    RequireServices(L);
    if (CGlueMgr::m_rereadScanning) {
        lua_pushnumber(L, 1.0);
    } else {
        lua_pushnil(L);
    }

    return 1;
}
static int32_t Script_AcceptScanning(lua_State* L) {
    RequireServices(L);
    CGlueMgr::AcceptScanning();
    return 0;
}
static int32_t Script_ContestAccepted(lua_State* L) {
    RequireServices(L);
    if (CGlueMgr::m_acceptedContest) {
        lua_pushnumber(L, 1.0);
    } else {
        lua_pushnil(L);
    }

    return 1;
}
static int32_t Script_ShowContestNotice(lua_State* L) {
    RequireServices(L);
    if (CGlueMgr::m_rereadContest) {
        lua_pushnumber(L, 1.0);
    } else {
        lua_pushnil(L);
    }

    return 1;
}
static int32_t Script_AcceptContest(lua_State* L) {
    RequireServices(L);
    CGlueMgr::AcceptContest();
    return 0;
}
void InitializeAgreements() {
    g_readTOSVar = CVar::Register("readTOS", "Status of the TOS", 0, "0", nullptr, CATEGORY_GAME, false, nullptr, false);
    CGlueMgr::m_acceptedTOS = g_readTOSVar->m_intValue == 1;
    CGlueMgr::m_rereadTOS = g_readTOSVar->m_intValue < 0;
    g_readEULAVar = CVar::Register("readEULA", "Status of the EULA", 0, "0", nullptr, CATEGORY_GAME, false, nullptr, false);
    CGlueMgr::m_acceptedEULA = g_readEULAVar->m_intValue == 1;
    CGlueMgr::m_rereadEULA = g_readEULAVar->m_intValue < 0;
    g_readTerminationWithoutNoticeVar = CVar::Register("readTerminationWithoutNotice", "Status of the Termination without Notice notice", 0, "0", nullptr, CATEGORY_GAME, false, nullptr, false);
    CGlueMgr::m_acceptedTerminationWithoutNotice = g_readTerminationWithoutNoticeVar->m_intValue == 1;
    CGlueMgr::m_rereadTerminationWithoutNotice = g_readTerminationWithoutNoticeVar->m_intValue < 0;
    g_readScanningVar = CVar::Register("readScanning", "Status of the Scanning notice", 0, "0", nullptr, CATEGORY_GAME, false, nullptr, false);
    CGlueMgr::m_acceptedScanning = g_readScanningVar->m_intValue == 1;
    CGlueMgr::m_rereadScanning = g_readScanningVar->m_intValue < 0;
    g_readContestVar = CVar::Register("readContest", "Status of the Contest notice", 0, "0", nullptr, CATEGORY_GAME, false, nullptr, false);
    CGlueMgr::m_acceptedContest = g_readContestVar->m_intValue == 1;
    CGlueMgr::m_rereadContest = g_readContestVar->m_intValue < 0;
}
void CloseAgreements() {
    g_readTOSVar = nullptr; CGlueMgr::m_acceptedTOS = CGlueMgr::m_rereadTOS = 0;
    g_readEULAVar = nullptr; CGlueMgr::m_acceptedEULA = CGlueMgr::m_rereadEULA = 0;
    g_readTerminationWithoutNoticeVar = nullptr; CGlueMgr::m_acceptedTerminationWithoutNotice = CGlueMgr::m_rereadTerminationWithoutNotice = 0;
    g_readScanningVar = nullptr; CGlueMgr::m_acceptedScanning = CGlueMgr::m_rereadScanning = 0;
    g_readContestVar = nullptr; CGlueMgr::m_acceptedContest = CGlueMgr::m_rereadContest = 0;
}

// Original Glue activation wrappers, retaining number/nil and zero-result APIs.
static int32_t Script_ShowChangedOptionWarnings(lua_State* L) {
    RequireServices(L);
    if (CGlueMgr::ShowChangedOptionWarnings()) lua_pushnumber(L, 1.0);
    else lua_pushnil(L);
    return 1;
}
static int32_t Script_GetChangedOptionWarnings(lua_State* L) {
    RequireServices(L);
    const auto count = CGlueMgr::m_numChangedOptionWarnings;
    if (!lua_checkstack(L, count)) return luaL_error(L, "Too many option warnings");
    for (uint32_t i = 0; i < count; ++i) lua_pushstring(L, CGlueMgr::m_changedOptionWarnings[i]);
    return count;
}
static int32_t Script_AcceptChangedOptionWarnings(lua_State* L) {
    RequireServices(L);
    CGlueMgr::AcceptChangedOptionWarnings(); return 0;
}
static int32_t Script_IsSystemSupported(lua_State* L) {
    RequireServices(L);
    // This original query tests SSE only, not overall native client readiness.
    if (OsGetProcessorFeatures() & 4) lua_pushnumber(L, 1.0);
    else lua_pushnil(L);
    return 1;
}
bool CharacterScreen(const char* screen) {
    return !SStrCmpI(screen, "charselect", STORM_MAX_STR) || !SStrCmpI(screen, "charcreate", STORM_MAX_STR);
}
static int32_t Script_SetCurrentScreen(lua_State* L) {
    RequireServices(L);
    if (!lua_isstring(L, 1)) return luaL_error(L, "Usage: SetCurrentScreen(\"screen\")");
    const char* screen = lua_tostring(L, 1);
    if (CharacterScreen(screen) || CharacterScreen(CGlueMgr::m_currentScreen)) {
#if defined(WOWDECOMP_NATIVE_TEST_SERVER_LOGIN)
        if (!testServerProfile || !testWorld || !testWorld->m_authenticated ||
            !SStrCmpI(screen,"charcreate",STORM_MAX_STR))
#endif
            return luaL_error(L, "Native character screen lifecycle unavailable");
    }
    CGlueMgr::UpdateCurrentScreen(screen);
    if (!SFile::IsStreamingMode()) AsyncFileReadWaitAll();
    return 0;
}

// OFFSET: 0x004DDD10
static int32_t Script_GetCVar(lua_State* L) {
    RequireServices(L);
    if (!lua_isstring(L, 1)) {
        luaL_error(L, "Usage: GetCVar(\"cvar\")");
        return 0;
    }

    CVar* var = CVar::LookupRegistered(lua_tolstring(L, 1, nullptr));

    if (!var) {
        luaL_error(L, "Couldn't find CVar named '%s'", lua_tolstring(L, 1, nullptr));
        return 0;
    }

    lua_pushstring(L, var->m_stringValue.m_str);
    return 1;
}

// OFFSET: 0x004DCE70
static int32_t Script_SetCVar(lua_State* L) {
    RequireServices(L);
    if (!lua_isstring(L, 1)) {
        luaL_error(L, "Usage: SetCVar(\"cvar\", value [, \"scriptCvar\")");
        return 0;
    }

    const char* name = lua_tolstring(L, 1, nullptr);
    CVar* var = CVar::LookupRegistered(name);

    if (!var) {
        luaL_error(L, "Couldn't find CVar named '%s'", name);
        return 0;
    }

    if (var->m_flags & 0x4) {
        luaL_error(L, "\"%s\" is read-only", name);
        return 0;
    }

    const char* value = lua_tolstring(L, 2, nullptr);

    if (!value) {
        value = "0";
    }

    var->Set(value, true, false, false, true);
    return 0;
}

// OFFSET: 0x004DDD90
static int32_t Script_GetCVarBool(lua_State* L) {
    RequireServices(L);
    if (!lua_isstring(L, 1)) {
        luaL_error(L, "Usage: GetCVarBool(\"cvar\")");
        return 0;
    }

    CVar* var = CVar::LookupRegistered(lua_tolstring(L, 1, nullptr));

    if (var && StringToBOOL(var->m_stringValue.m_str)) {
        lua_pushnumber(L, 1.0);
    } else {
        lua_pushnil(L);
    }

    return 1;
}

// OFFSET: 0x004DDE10
static int32_t Script_GetCVarDefault(lua_State* L) {
    RequireServices(L);
    if (!lua_isstring(L, 1)) {
        luaL_error(L, "Usage: GetCVarDefault(\"cvar\")");
        return 0;
    }

    CVar* var = CVar::LookupRegistered(lua_tolstring(L, 1, nullptr));

    if (!var) {
        luaL_error(L, "Couldn't find CVar named '%s'", lua_tolstring(L, 1, nullptr));
        return 0;
    }

    if (var->m_defaultValue.m_str) {
        lua_pushstring(L, var->m_defaultValue.m_str);
    } else {
        lua_pushnil(L);
    }

    return 1;
}

// OFFSET: 0x004DDEA0
static int32_t Script_GetCVarMin(lua_State* L) {
    RequireServices(L);
    if (!lua_isstring(L, 1)) {
        luaL_error(L, "Usage: GetCVarMin(\"cvar\")");
        return 0;
    }

    CVar* var = CVar::LookupRegistered(lua_tolstring(L, 1, nullptr));

    if (!var) {
        luaL_error(L, "Couldn't find CVar named '%s'", lua_tolstring(L, 1, nullptr));
        return 0;
    }

    const char* name = var->m_key.m_str;

    if (!SStrCmpI(name, "extShadowQuality", STORM_MAX_STR)) {
        lua_pushnumber(L, 0.0);
        return 1;
    }

    if (!SStrCmpI(name, "farclip", STORM_MAX_STR)) {
        lua_pushnumber(L, 177.0);
    } else {
        lua_pushnil(L);
    }

    return 1;
}

// OFFSET: 0x004DDF80
static int32_t Script_GetCVarMax(lua_State* L) {
    RequireServices(L);
    if (!lua_isstring(L, 1)) {
        luaL_error(L, "Usage: GetCVarMax(\"cvar\")");
        return 0;
    }

    CVar* var = CVar::LookupRegistered(lua_tolstring(L, 1, nullptr));

    if (!var) {
        luaL_error(L, "Couldn't find CVar named '%s'", lua_tolstring(L, 1, nullptr));
        return 0;
    }

    const char* name = var->m_key.m_str;

    if (!SStrCmpI(name, "extShadowQuality", STORM_MAX_STR)) {
        uint32_t mode = 0;
        uint32_t maxMode = CShadowCache::GetMaxShadowMode();

        do {
            if (!CShadowCache::ValidateShadowMode(static_cast<EShadowMode>(mode))) {
                break;
            }

            mode++;
        } while (mode <= maxMode);

        lua_pushnumber(L, mode - 1);
        return 1;
    }

    if (!SStrCmpI(name, "farclip", STORM_MAX_STR)) {
        lua_pushnumber(L, 1277.0);
    } else {
        lua_pushnil(L);
    }

    return 1;
}

// OFFSET: 0x004DE090
static int32_t Script_GetCVarAbsoluteMin(lua_State* L) {
    RequireServices(L);
    if (!lua_isstring(L, 1)) {
        luaL_error(L, "Usage: GetCVarAbsoluteMin(\"cvar\")");
        return 0;
    }

    CVar* var = CVar::LookupRegistered(lua_tolstring(L, 1, nullptr));

    if (!var) {
        luaL_error(L, "Couldn't find CVar named '%s'", lua_tolstring(L, 1, nullptr));
        return 0;
    }

    if (SStrCmpI(var->m_key.m_str, "extShadowQuality", STORM_MAX_STR)) {
        lua_pushnil(L);
    } else {
        lua_pushnumber(L, 0.0);
    }

    return 1;
}

// OFFSET: 0x004DE130
static int32_t Script_GetCVarAbsoluteMax(lua_State* L) {
    RequireServices(L);
    if (!lua_isstring(L, 1)) {
        luaL_error(L, "Usage: GetCVarAbsoluteMax(\"cvar\")");
        return 0;
    }

    CVar* var = CVar::LookupRegistered(lua_tolstring(L, 1, nullptr));

    if (!var) {
        luaL_error(L, "Couldn't find CVar named '%s'", lua_tolstring(L, 1, nullptr));
        return 0;
    }

    if (SStrCmpI(var->m_key.m_str, "extShadowQuality", STORM_MAX_STR)) {
        lua_pushnil(L);
    } else {
        lua_pushnumber(L, CShadowCache::GetMaxShadowMode());
    }

    return 1;
}

// OFFSET: 0x004DCC20
static int32_t Script_GetScreenWidth(lua_State* L) {
    RequireServices(L);
    float width = NDCToDDCWidth(1.0f);
    lua_pushnumber(L, DDCToNDCWidth(CoordinateGetAspectCompensation() * 1024.0f * width));
    return 1;
}

// OFFSET: 0x004DCC70
static int32_t Script_GetScreenHeight(lua_State* L) {
    RequireServices(L);
    float height = NDCToDDCHeight(1.0f);
    lua_pushnumber(L, DDCToNDCWidth(CoordinateGetAspectCompensation() * 1024.0f * height));
    return 1;
}

// OFFSET: 0x004DD490
static int32_t Script_RunScript(lua_State* L) {
    RequireServices(L);
    if (lua_isstring(L, 1)) {
        const char* script = lua_tolstring(L, 1, nullptr);

        if (script && *script) {
            FrameScript_Execute(script, script, nullptr);
        }
    }

    return 0;
}

// OFFSET: 0x004DD430
static int32_t Script_IsStreamingMode(lua_State* L) {
    RequireServices(L);
    return SFile::IsStreamingMode();
}

// OFFSET: 0x004DD440
static int32_t Script_IsStreamingTrial(lua_State* L) {
    RequireServices(L);
    return SFile::IsTrial();
}
int32_t Script_GetLocale(lua_State* L) {
    RequireServices(L);
    lua_pushstring(L, "enUS"); return 1; // Explicit locale of the integrated target.
}
int32_t Script_OtherPlatform(lua_State* L) {
    RequireServices(L); lua_pushnil(L); return 1;
}
int32_t Script_IsLinuxClient(lua_State* L) {
    RequireServices(L); lua_pushnumber(L, 1.0); return 1;
}
// CGlueMgr::QuitGame (0x004D8350) posts close to the installed native Event owner.
int32_t Script_QuitGame(lua_State* L) {
    RequireServices(L); EventPostClose(); return 0;
}
// Original Glue Script_GetClientExpansionLevel (0x004DCBF0), not the account query.
int32_t Script_GetClientExpansionLevel(lua_State* L) {
    RequireServices(L);
    lua_pushnumber(L, clientExpansionLevel + 1); return 1;
}
FrameScript_Method functions[] = {
#if defined(WOWDECOMP_NATIVE_TEST_SERVER_LOGIN)
    { "GetAccountExpansionLevel", Script_GetAccountExpansionLevel },
    { "IsConnectedToServer", Script_IsConnectedToServer },
    { "DisconnectFromServer", Script_DisconnectFromServer },
    { "EnterWorld", Script_EnterWorld },
#endif
    { "ScanDLLStart", Script_ScanDLLStart },
    { "ScanDLLContinueAnyway", Script_ScanDLLContinueAnyway },
    { "IsScanDLLFinished", Script_IsScanDLLFinished },
    { "DefaultServerLogin", Script_DefaultServerLogin },
    { "StatusDialogClick", Script_StatusDialogClick },
    { "GetBuildInfo", Script_GetBuildInfo },
    { "GetClientExpansionLevel", Script_GetClientExpansionLevel },
    { "SetSavedAccountName", Script_SetSavedAccountName },
    { "GetUsesToken", Script_GetUsesToken },
    { "SetUsesToken", Script_SetUsesToken },
    { "SetSavedAccountList", Script_SetSavedAccountList },
    { "GetSavedAccountName", Script_GetSavedAccountName },
    { "GetSavedAccountList", Script_GetSavedAccountList },
    { "ShowChangedOptionWarnings", Script_ShowChangedOptionWarnings },
    { "GetChangedOptionWarnings", Script_GetChangedOptionWarnings },
    { "AcceptChangedOptionWarnings", Script_AcceptChangedOptionWarnings },
    { "IsSystemSupported", Script_IsSystemSupported },
    { "SetCurrentScreen", Script_SetCurrentScreen },
    { "GetServerName", Script_GetServerName },
    { "IsTrialAccount", Script_IsTrialAccount },
    { "TOSAccepted", Script_TOSAccepted },
    { "ShowTOSNotice", Script_ShowTOSNotice },
    { "AcceptTOS", Script_AcceptTOS },
    { "EULAAccepted", Script_EULAAccepted },
    { "ShowEULANotice", Script_ShowEULANotice },
    { "AcceptEULA", Script_AcceptEULA },
    { "TerminationWithoutNoticeAccepted", Script_TerminationWithoutNoticeAccepted },
    { "ShowTerminationWithoutNoticeNotice", Script_ShowTerminationWithoutNoticeNotice },
    { "AcceptTerminationWithoutNotice", Script_AcceptTerminationWithoutNotice },
    { "ScanningAccepted", Script_ScanningAccepted },
    { "ShowScanningNotice", Script_ShowScanningNotice },
    { "AcceptScanning", Script_AcceptScanning },
    { "ContestAccepted", Script_ContestAccepted },
    { "ShowContestNotice", Script_ShowContestNotice },
    { "AcceptContest", Script_AcceptContest },
    { "GetCVar", Script_GetCVar },
    { "SetCVar", Script_SetCVar },
    { "GetCVarBool", Script_GetCVarBool },
    { "GetCVarDefault", Script_GetCVarDefault },
    { "GetCVarMin", Script_GetCVarMin },
    { "GetCVarMax", Script_GetCVarMax },
    { "GetCVarAbsoluteMin", Script_GetCVarAbsoluteMin },
    { "GetCVarAbsoluteMax", Script_GetCVarAbsoluteMax },
    { "GetScreenWidth", Script_GetScreenWidth },
    { "GetScreenHeight", Script_GetScreenHeight },
    { "RunScript", Script_RunScript },
    { "IsStreamingMode", Script_IsStreamingMode },
    { "IsStreamingTrial", Script_IsStreamingTrial },
    { "GetLocale", Script_GetLocale },
    { "IsMacClient", Script_OtherPlatform },
    { "IsWindowsClient", Script_OtherPlatform },
    { "IsLinuxClient", Script_IsLinuxClient },
    { "QuitGame", Script_QuitGame },
};
}
// Original CGlueMgr agreement transitions. Only explicit callbacks enter these;
// startup reads saved state and displays prompts without invoking acceptance.
void CGlueMgr::AcceptTOS() {
    FrameScript::Native::RequireOwner();
    if (!registered || !CVar::m_initialized || !g_readTOSVar ||
        CVar::LookupRegistered("readTOS") != g_readTOSVar)
        throw std::logic_error("Native agreement owner is closed");
    int32_t value = g_readTOSVar->m_intValue;
    CGlueMgr::m_acceptedTOS = 1;
    CGlueMgr::m_rereadTOS = 0;
    if (value == -2) {
        g_readTOSVar->Set("-1", true, false, false, true);
    } else {
        g_readTOSVar->Set("1", true, false, false, true);
    }
}
void CGlueMgr::AcceptEULA() {
    FrameScript::Native::RequireOwner();
    if (!registered || !CVar::m_initialized || !g_readEULAVar ||
        CVar::LookupRegistered("readEULA") != g_readEULAVar)
        throw std::logic_error("Native agreement owner is closed");
    int32_t value = g_readEULAVar->m_intValue;
    CGlueMgr::m_acceptedEULA = 1;
    CGlueMgr::m_rereadEULA = 0;
    if (value == -2) {
        g_readEULAVar->Set("-1", true, false, false, true);
    } else {
        g_readEULAVar->Set("1", true, false, false, true);
    }
}
void CGlueMgr::AcceptTerminationWithoutNotice() {
    FrameScript::Native::RequireOwner();
    if (!registered || !CVar::m_initialized || !g_readTerminationWithoutNoticeVar ||
        CVar::LookupRegistered("readTerminationWithoutNotice") != g_readTerminationWithoutNoticeVar)
        throw std::logic_error("Native agreement owner is closed");
    int32_t value = g_readTerminationWithoutNoticeVar->m_intValue;
    CGlueMgr::m_acceptedTerminationWithoutNotice = 1;
    CGlueMgr::m_rereadTerminationWithoutNotice = 0;
    if (value == -2) {
        g_readTerminationWithoutNoticeVar->Set("-1", true, false, false, true);
    } else {
        g_readTerminationWithoutNoticeVar->Set("1", true, false, false, true);
    }
}
void CGlueMgr::AcceptScanning() {
    FrameScript::Native::RequireOwner();
    if (!registered || !CVar::m_initialized || !g_readScanningVar ||
        CVar::LookupRegistered("readScanning") != g_readScanningVar)
        throw std::logic_error("Native agreement owner is closed");
    CGlueMgr::m_acceptedScanning = 1;
    CGlueMgr::m_rereadScanning = 0;
    g_readScanningVar->Set("1", true, false, false, true);
}
void CGlueMgr::AcceptContest() {
    FrameScript::Native::RequireOwner();
    if (!registered || !CVar::m_initialized || !g_readContestVar ||
        CVar::LookupRegistered("readContest") != g_readContestVar)
        throw std::logic_error("Native agreement owner is closed");
    CGlueMgr::m_acceptedContest = 1;
    CGlueMgr::m_rereadContest = 0;
    g_readContestVar->Set("1", true, false, false, true);
}

// Selected original warning/lifecycle operations from CGlueMgr.cpp. Character
// screens require their real OnHide/Idle services before entering this surface.
void CGlueMgr::UpdateCurrentScreen(const char* screen) {
    FrameScript::Native::RequireOwner();
#if defined(WOWDECOMP_NATIVE_TEST_SERVER_LOGIN)
    if (screen && !SStrCmpI(screen,"charselect",STORM_MAX_STR) &&
        testServerProfile && testWorld && testWorld->m_authenticated) {
        if (!SStrCmpI(m_currentScreen,"charselect",STORM_MAX_STR)) CCharacterSelection::OnHide();
        EventRegisterEx(EVENT_ID_IDLE,CCharacterSelection::Idle,nullptr,EVENT_PRIORITY_NORMAL);
        SStrCopy(m_currentScreen,screen,sizeof(m_currentScreen));
        return;
    }
    if (screen && !SStrCmpI(m_currentScreen,"charselect",STORM_MAX_STR) && !CharacterScreen(screen)) {
        CCharacterSelection::OnHide();
        SStrCopy(m_currentScreen,screen,sizeof(m_currentScreen));
        return;
    }
#endif
    if (!screen || CharacterScreen(screen) || CharacterScreen(m_currentScreen))
        throw std::logic_error("Native character screen lifecycle unavailable");
    SStrCopy(m_currentScreen, screen, sizeof(m_currentScreen));
}
int32_t CGlueMgr::ShowChangedOptionWarnings() {
    FrameScript::Native::RequireOwner(); return m_numChangedOptionWarnings != 0;
}
void CGlueMgr::AcceptChangedOptionWarnings() {
    FrameScript::Native::RequireOwner();
    m_changedOptionWarnings.Clear(); m_numChangedOptionWarnings = 0; warningText.clear();
}
void CGlueMgr::AddChangedOptionWarning(const char* warning) {
    FrameScript::Native::RequireOwner();
    if (!warning) return;
    const char* text = FrameScript_GetText(warning, -1, GENDER_NOT_APPLICABLE);
    if (!text) return;
    if (m_numChangedOptionWarnings >= 256) throw std::length_error("Native option warning limit");
    warningText.emplace_back(text);
    try { m_changedOptionWarnings.New(warningText.back().c_str()); }
    catch (...) { warningText.pop_back(); throw; }
    ++m_numChangedOptionWarnings;
}
#if defined(WOWDECOMP_NATIVE_TEST_SERVER_LOGIN)
// Original realm/character API calls defer connection/UI work until outer idle.
void CGlueMgr::GetRealmList(int32_t) {
    FrameScript::Native::RequireOwner();
    if (!testServerProfile || !testLogin || testLogin->m_logonStatus != 0)
        throw std::logic_error("Realm list requires verified test-server authentication");
    if (testConnectPending || (testWorldStarted && !testWorld->m_statusComplete)) return;
    if (!testLogin->IsLoggedOn()) {
        pendingDialog = "TEST SERVER: login connection expired. Sign in again to refresh realms; reconnect is unavailable.";
        return;
    }
    testRealmRequestPending = true;
}
void CancelRealmListQuery() {
    FrameScript::Native::RequireOwner();
    testRealmRequestPending = false;
}
void CGlueMgr::RealmListDialogCancelled() {
    FrameScript::Native::RequireOwner();
    if (!SStrCmpI(m_currentScreen,"login",STORM_MAX_STR)) testCancelPending = true;
}
void CGlueMgr::ChangeRealm(REALM_INFO* realm) {
    FrameScript::Native::RequireOwner();
    if (!testServerProfile || !testRealmsReady || !realm ||
        !ClientServices::SelectRealm(realm->m_realmName))
        throw std::logic_error("Selected realm is unavailable or incompatible");
    auto* name = CVar::Register("realmName", "Last realm connected to", 0, "", nullptr, CATEGORY_NET, false, nullptr, false);
    name->Set(realm->m_realmName,true,false,false,true);
    testConnectPending = true;
}
void CGlueMgr::GetCharacterList() {
    FrameScript::Native::RequireOwner();
    if (!testServerProfile || !testWorld || !testWorld->m_authenticated)
        throw std::logic_error("Character list requires actual world authentication");
    if (testPublishingCharacters) return; // A fresh server enumeration is being published.
    testCharacterRequestPending = true;
}
#endif
namespace GlueNative {
void InitializeScriptServices(uint8_t expansionLevel, bool testServerLogin) {
    FrameScript::Native::RequireOwner();
    if (expansionLevel > 2) throw std::invalid_argument("Invalid original client expansion level");
    if (registered || !CVar::m_initialized) throw std::logic_error("Register Glue CVars inside their live owner before scripts");
#if defined(WOWDECOMP_NATIVE_TEST_SERVER_LOGIN)
    testServerProfile = false;
    testEndpoint.clear();
    if (testServerLogin) {
        auto* endpoint = CVar::Register("realmlist", "External isolated test auth server", 0, "", nullptr, CATEGORY_NET, false, nullptr, false);
        NativeTestServer::ValidateEndpoint(endpoint->m_stringValue.m_str);
        testEndpoint = endpoint->m_stringValue.m_str;
        testServerProfile = true;
        std::printf("native-login-profile=isolated-test-server identity=Linux-x86_64 integrity=unavailable warden=unavailable\n");
    }
#else
    if (testServerLogin) throw std::invalid_argument("Test-server login was not compiled into this build");
#endif
    clientExpansionLevel = expansionLevel;
    scanState = ScanState::NotStarted;
    CGlueMgr::m_authenticated = 0;
    InitializeAgreements();
    CGlueMgr::m_currentScreen[0] = '\0';
    CGlueMgr::AcceptChangedOptionWarnings();
    // Client_CVar.cpp::ClientRegisterConsoleCommands (12340 0x00401B60).
    // CVar owns these entries and preserves values read from the external config.
    g_accountNameVar = CVar::Register("accountName", "Saved account name", 0x40, "", nullptr, CATEGORY_GAME, false, nullptr, false);
    g_accountListVar = CVar::Register("accountList", "List of wow accounts for saved Blizzard account", 0, "", nullptr, CATEGORY_GAME, false, nullptr, false);
    g_accountUsesTokenVar = CVar::Register("g_accountUsesToken", "Saved whether uses authenticator", 0, "0", nullptr, CATEGORY_GAME, false, nullptr, false);
    CVar* showToolsUI = CVar::Register("showToolsUI", "Display the launcher when starting the game", 0, "-1", nullptr, CATEGORY_GAME, false, nullptr, false);
    if (static_cast<uint32_t>(showToolsUI->m_intValue) >= 2)
        showToolsUI->Set("1", true, false, false, true);
}
void PollScriptServices() {
    FrameScript::Native::RequireOwner();
    if (scanDispatching) throw std::logic_error("Finish ScanDLL event dispatch before polling Glue services");
    if (!ServicesOpen()) throw std::logic_error("Native Glue script services are closed");
#if defined(WOWDECOMP_NATIVE_TEST_SERVER_LOGIN)
    if (testServerProfile) {
        struct Dispatch { Dispatch() { scanDispatching = true; } ~Dispatch() { scanDispatching = false; } } dispatch;
        PollTestLogin();
    }
#endif
    const bool scanFailed = scanState == ScanState::ErrorPending;
    if (!scanFailed && !pendingDialog) return;
    const char* reason = scanFailed ? ScanNotice() : pendingDialog;
    pendingDialog = nullptr;
    // Publish the terminal state before dispatch. Repeated starts/polls cannot
    // duplicate the event, including when its protected Lua callback fails.
    if (scanFailed) {
        scanState = ScanState::Unavailable;
        std::printf("native-unavailable=scandll reason=%s\n", scanUnavailable);
    }
    struct Dispatch {
        Dispatch() { scanDispatching = true; }
        ~Dispatch() { scanDispatching = false; }
    } dispatch;
    const auto errors = FrameScript::Native::ScriptErrorCount();
    if (scanFailed) FrameScript_SignalEvent(GLUE_EVENT_SCANDLL_ERROR, "%s", scanUnavailable);
    // The original handler has now shown AccountLoginUI. Present the actual
    // failure through its original dialog, without a fabricated FINISHED/OK.
    if (FrameScript::Native::ScriptErrorCount() == errors) ShowUnavailable(reason);
    else pendingDialog = nullptr;
}
}
void GlueScriptEventsRegisterFunctions() {
    FrameScript::Native::RequireOwner();
    if (registered || !CVar::m_initialized || !g_accountNameVar || !g_accountListVar || !g_accountUsesTokenVar)
        throw std::logic_error("Register original Glue console variables before scripts");
    for (const auto& function : functions) FrameScript_RegisterFunction(function.m_name, function.m_method);
#if defined(WOWDECOMP_NATIVE_TEST_SERVER_LOGIN)
    if (testServerProfile) RealmListRegisterScriptFunctions();
#endif
    registered = true;
}
void GlueScriptEventsUnregisterFunctions() {
    FrameScript::Native::RequireOwner();
    if (scanDispatching) throw std::logic_error("Finish ScanDLL event dispatch before closing Glue services");
#if defined(WOWDECOMP_NATIVE_TEST_SERVER_LOGIN)
    if (testServerProfile) RealmListUnregisterScriptFunctions();
    ResetTestLogin();
    testServerProfile = testDialog = false;
    testEndpoint.clear();
#endif
    for (const auto& function : functions) FrameScript_UnregisterFunction(function.m_name);
    registered = false;
    scanState = ScanState::NotStarted;
    unavailableDialog = false;
    pendingDialog = nullptr;
    CGlueMgr::m_authenticated = 0;
    CloseAgreements();
    CGlueMgr::AcceptChangedOptionWarnings();
    CGlueMgr::m_currentScreen[0] = '\0';
    g_accountNameVar = g_accountListVar = g_accountUsesTokenVar = nullptr;
}
