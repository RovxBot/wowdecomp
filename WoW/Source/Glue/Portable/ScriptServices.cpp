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
#include "GameVersion.h"
#include <deque>
#include <string>
#include <stdexcept>
// Native selection of original CGlueMgr activation state. Warning text is owned
// until acknowledgement/close, rather than borrowing collectable Lua strings.
char CGlueMgr::m_currentScreen[64];
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
void RequireServices(lua_State* L) {
    FrameScript::Native::RequireOwner();
    if (!registered || !CVar::m_initialized ||
        CVar::LookupRegistered("accountName") != g_accountNameVar ||
        CVar::LookupRegistered("accountList") != g_accountListVar ||
        CVar::LookupRegistered("g_accountUsesToken") != g_accountUsesTokenVar)
        luaL_error(L, "Native Glue script services are closed");
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
    if (CharacterScreen(screen) || CharacterScreen(CGlueMgr::m_currentScreen))
        return luaL_error(L, "Native character screen lifecycle unavailable");
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
namespace GlueNative {
void InitializeScriptServices(uint8_t expansionLevel) {
    FrameScript::Native::RequireOwner();
    if (expansionLevel > 2) throw std::invalid_argument("Invalid original client expansion level");
    if (registered || !CVar::m_initialized) throw std::logic_error("Register Glue CVars inside their live owner before scripts");
    clientExpansionLevel = expansionLevel;
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
}
void GlueScriptEventsRegisterFunctions() {
    FrameScript::Native::RequireOwner();
    if (registered || !CVar::m_initialized || !g_accountNameVar || !g_accountListVar || !g_accountUsesTokenVar)
        throw std::logic_error("Register original Glue console variables before scripts");
    for (const auto& function : functions) FrameScript_RegisterFunction(function.m_name, function.m_method);
    registered = true;
}
void GlueScriptEventsUnregisterFunctions() {
    FrameScript::Native::RequireOwner();
    for (const auto& function : functions) FrameScript_UnregisterFunction(function.m_name);
    registered = false;
    CloseAgreements();
    CGlueMgr::AcceptChangedOptionWarnings();
    CGlueMgr::m_currentScreen[0] = '\0';
    g_accountNameVar = g_accountListVar = g_accountUsesTokenVar = nullptr;
}
