// Selected original 12340 GlueScriptEvents_Scripts.cpp services. Native-only
// adaptation: retained CVar/FrameScript owners, mounted local packages and Event
// shutdown. Original world GetBuildInfo is a separate four-result API.
#include "ScriptServices.h"
#include "WoW/Source/Glue/GlueMgr.h"
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
#include "GameVersion.h"
namespace {
CVar* g_accountNameVar;
CVar* g_accountListVar;
CVar* g_accountUsesTokenVar;
// OFFSET: 0x004DBE60
static int32_t Script_GetBuildInfo(lua_State* L) {
    FrameScript::Native::RequireOwner();
    lua_pushstring(L, FrameScript_GetText("VERSION", -1, GENDER_NOT_APPLICABLE));
    lua_pushstring(L, FrameScript_GetText("RELEASE_BUILD", -1, GENDER_NOT_APPLICABLE));
    lua_pushstring(L, GAME_VERSION);
    lua_pushstring(L, GAME_BUILD_STRING);
    lua_pushstring(L, GAME_BUILD_DATE);
    return 5;
}

// OFFSET: 0x004DBEC0
static int32_t Script_SetSavedAccountName(lua_State* L) {
    FrameScript::Native::RequireOwner();
    if (!lua_isstring(L, 1)) {
        luaL_error(L, "Usage: SetSavedAccountName(\"accountName\")");
        return 0;
    }

    g_accountNameVar->Set(lua_tolstring(L, 1, nullptr), true, false, false, true);
    return 0;
}

// OFFSET: 0x004DBF10
static int32_t Script_GetUsesToken(lua_State* L) {
    FrameScript::Native::RequireOwner();
    lua_pushboolean(L, g_accountUsesTokenVar->m_intValue);
    return 1;
}

// OFFSET: 0x004DBF30
static int32_t Script_SetUsesToken(lua_State* L) {
    FrameScript::Native::RequireOwner();
    if (lua_type(L, 1) != LUA_TBOOLEAN) {
        luaL_error(L, "Usage: SetUsesToken( 0 | 1 )");
        return 0;
    }

    g_accountUsesTokenVar->Set(lua_toboolean(L, 1), true, false, false, true);
    return 0;
}

// OFFSET: 0x004DBF80
static int32_t Script_SetSavedAccountList(lua_State* L) {
    FrameScript::Native::RequireOwner();
    if (!lua_isstring(L, 1)) {
        luaL_error(L, "Usage: SetSavedAccountList(\"accountList\")");
        return 0;
    }

    g_accountListVar->Set(lua_tolstring(L, 1, nullptr), true, false, false, true);
    return 0;
}

// OFFSET: 0x004DD5D0
static int32_t Script_GetSavedAccountName(lua_State* L) {
    FrameScript::Native::RequireOwner();
    lua_pushstring(L, g_accountNameVar->m_stringValue.m_str);
    return 1;
}

// OFFSET: 0x004DD5F0
static int32_t Script_GetSavedAccountList(lua_State* L) {
    FrameScript::Native::RequireOwner();
    lua_pushstring(L, g_accountListVar->m_stringValue.m_str);
    return 1;
}

// OFFSET: 0x004DDD10
static int32_t Script_GetCVar(lua_State* L) {
    FrameScript::Native::RequireOwner();
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
    FrameScript::Native::RequireOwner();
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
    FrameScript::Native::RequireOwner();
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
    FrameScript::Native::RequireOwner();
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
    FrameScript::Native::RequireOwner();
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
    FrameScript::Native::RequireOwner();
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
    FrameScript::Native::RequireOwner();
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
    FrameScript::Native::RequireOwner();
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
    FrameScript::Native::RequireOwner();
    float width = NDCToDDCWidth(1.0f);
    lua_pushnumber(L, DDCToNDCWidth(CoordinateGetAspectCompensation() * 1024.0f * width));
    return 1;
}

// OFFSET: 0x004DCC70
static int32_t Script_GetScreenHeight(lua_State* L) {
    FrameScript::Native::RequireOwner();
    float height = NDCToDDCHeight(1.0f);
    lua_pushnumber(L, DDCToNDCWidth(CoordinateGetAspectCompensation() * 1024.0f * height));
    return 1;
}

// OFFSET: 0x004DD490
static int32_t Script_RunScript(lua_State* L) {
    FrameScript::Native::RequireOwner();
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
    FrameScript::Native::RequireOwner();
    return SFile::IsStreamingMode();
}

// OFFSET: 0x004DD440
static int32_t Script_IsStreamingTrial(lua_State* L) {
    FrameScript::Native::RequireOwner();
    return SFile::IsTrial();
}
int32_t Script_GetLocale(lua_State* L) {
    FrameScript::Native::RequireOwner();
    lua_pushstring(L, "enUS"); return 1; // Explicit locale of the integrated target.
}
int32_t Script_OtherPlatform(lua_State* L) {
    FrameScript::Native::RequireOwner(); lua_pushnil(L); return 1;
}
int32_t Script_IsLinuxClient(lua_State* L) {
    FrameScript::Native::RequireOwner(); lua_pushnumber(L, 1.0); return 1;
}
// CGlueMgr::QuitGame (0x004D8350) posts close to the installed native Event owner.
int32_t Script_QuitGame(lua_State*) {
    FrameScript::Native::RequireOwner(); EventPostClose(); return 0;
}
FrameScript_Method functions[] = {
    { "GetBuildInfo", Script_GetBuildInfo },
    { "SetSavedAccountName", Script_SetSavedAccountName },
    { "GetUsesToken", Script_GetUsesToken },
    { "SetUsesToken", Script_SetUsesToken },
    { "SetSavedAccountList", Script_SetSavedAccountList },
    { "GetSavedAccountName", Script_GetSavedAccountName },
    { "GetSavedAccountList", Script_GetSavedAccountList },
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
namespace GlueNative {
void RegisterConsoleVariables() {
    FrameScript::Native::RequireOwner();
    // Client_CVar.cpp::ClientRegisterConsoleCommands (12340 0x00401B60).
    // CVar owns these entries and preserves values read from the external config.
    g_accountNameVar = CVar::Register("accountName", "Saved account name", 0x40, "", nullptr, CATEGORY_GAME, false, nullptr, false);
    g_accountListVar = CVar::Register("accountList", "List of wow accounts for saved Blizzard account", 0, "", nullptr, CATEGORY_GAME, false, nullptr, false);
    g_accountUsesTokenVar = CVar::Register("g_accountUsesToken", "Saved whether uses authenticator", 0, "0", nullptr, CATEGORY_GAME, false, nullptr, false);
}
}
void GlueScriptEventsRegisterFunctions() {
    FrameScript::Native::RequireOwner();
    for (const auto& function : functions) FrameScript_RegisterFunction(function.m_name, function.m_method);
}
void GlueScriptEventsUnregisterFunctions() {
    FrameScript::Native::RequireOwner();
    for (const auto& function : functions) FrameScript_UnregisterFunction(function.m_name);
    g_accountNameVar = g_accountListVar = g_accountUsesTokenVar = nullptr;
}
