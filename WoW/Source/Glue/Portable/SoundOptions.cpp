// Original SI2 sound preferences and driver-query contracts in the Linux
// startup source group. There is no native sound system yet: original SE2's
// no-system path reports zero drivers and fails name reads. Playback, restart,
// capture and hardware-changing handlers are not selected here.
#include "WoW/Source/SoundInterface/SoundInterface2.h"
#include "Engine/Source/ConsoleBase/ConsoleVar.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/FrameScript/FrameScript_Object.h"
#include "Engine/Source/FrameScript/Portable/Runtime.h"
#include "Engine/Source/Os/System.h"
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>

int32_t SI2::GetNumInputDrivers(int32_t) { return 0; }
int32_t SI2::GetNumInputDrivers_Cached(int32_t) { return 0; }
int32_t SI2::GetNumOutputDrivers_Cached(int32_t) { return 0; }
bool SI2::GetInputDriverName(int32_t, char*, int32_t, int32_t) { return false; }
bool SI2::GetInputDriverName_Cached(int32_t, char*, int32_t, int32_t) { return false; }
bool SI2::GetOutputDriverName_Cached(int32_t, char*, int32_t, int32_t) { return false; }
namespace {
bool registered = false;
void RequireSoundQueries(lua_State* L) {
    FrameScript::Native::RequireOwner();
    if (!registered || !CVar::m_initialized) luaL_error(L, "Native sound-option queries are closed");
}
static int32_t Script_Sound_ChatSystem_GetNumInputDrivers(lua_State* L) {
    RequireSoundQueries(L);
    lua_pushnumber(L, SI2::GetNumInputDrivers_Cached(1));
    return 1;
}

static int32_t Script_Sound_ChatSystem_GetInputDriverNameByIndex(lua_State* L) {
    RequireSoundQueries(L);
    if (!lua_isnumber(L, 1)) {
        luaL_error(L, "Usage: Sound_ChatSystem_GetInputDriverNameByIndex(InputDriverIndex)");
        return 0;
    }

    const auto value = lua_tonumber(L, 1);
    if (!std::isfinite(value) || value < INT32_MIN || value > INT32_MAX)
        return luaL_error(L, "Sound driver index is outside the original integer range");
    int32_t index = static_cast<int32_t>(value);
    char name[2048];
    memset(name, 0, sizeof(name));
    SI2::GetInputDriverName_Cached(index, name, sizeof(name), 1);
    lua_pushstring(L, name);
    return 1;
}

static int32_t Script_Sound_ChatSystem_GetNumOutputDrivers(lua_State* L) {
    RequireSoundQueries(L);
    lua_pushnumber(L, SI2::GetNumOutputDrivers_Cached(1));
    return 1;
}

static int32_t Script_Sound_ChatSystem_GetOutputDriverNameByIndex(lua_State* L) {
    RequireSoundQueries(L);
    if (!lua_isnumber(L, 1)) {
        luaL_error(L, "Usage: Sound_ChatSystem_GetOutputDriverNameByIndex(OutputDriverIndex)");
        return 0;
    }

    const auto value = lua_tonumber(L, 1);
    if (!std::isfinite(value) || value < INT32_MIN || value > INT32_MAX)
        return luaL_error(L, "Sound driver index is outside the original integer range");
    int32_t index = static_cast<int32_t>(value);
    char name[2048];
    memset(name, 0, sizeof(name));

    if (index >= 0 && index <= SI2::GetNumOutputDrivers_Cached(1)) {
        SI2::GetOutputDriverName_Cached(index, name, sizeof(name), 1);
    }

    lua_pushstring(L, name);
    return 1;
}

static int32_t Script_Sound_GameSystem_GetNumInputDrivers(lua_State* L) {
    RequireSoundQueries(L);
    lua_pushnumber(L, SI2::GetNumInputDrivers(0));
    return 1;
}

static int32_t Script_Sound_GameSystem_GetInputDriverNameByIndex(lua_State* L) {
    RequireSoundQueries(L);
    if (!lua_isnumber(L, 1)) {
        luaL_error(L, "Usage: Sound_GameSystem_GetInputDriverNameByIndex(InputDriverIndex)");
        return 0;
    }

    const auto value = lua_tonumber(L, 1);
    if (!std::isfinite(value) || value < INT32_MIN || value > INT32_MAX)
        return luaL_error(L, "Sound driver index is outside the original integer range");
    int32_t index = static_cast<int32_t>(value);
    char name[2048];
    memset(name, 0, sizeof(name));
    SI2::GetInputDriverName(index, name, sizeof(name), 0);
    lua_pushstring(L, name);
    return 1;
}

static int32_t Script_Sound_GameSystem_GetNumOutputDrivers(lua_State* L) {
    RequireSoundQueries(L);
    lua_pushnumber(L, SI2::GetNumOutputDrivers_Cached(0));
    return 1;
}

static int32_t Script_Sound_GameSystem_GetOutputDriverNameByIndex(lua_State* L) {
    RequireSoundQueries(L);
    if (!lua_isnumber(L, 1)) {
        luaL_error(L, "Usage: Sound_GetOutputDriverNameByIndex(OutputDriverIndex)");
        return 0;
    }

    const auto value = lua_tonumber(L, 1);
    if (!std::isfinite(value) || value < INT32_MIN || value > INT32_MAX)
        return luaL_error(L, "Sound driver index is outside the original integer range");
    int32_t index = static_cast<int32_t>(value);
    char name[2048];
    memset(name, 0, sizeof(name));
    SI2::GetOutputDriverName_Cached(index, name, sizeof(name), 0);
    lua_pushstring(L, name);
    return 1;
}
FrameScript_Method functions[] = {
    { "Sound_ChatSystem_GetNumInputDrivers", Script_Sound_ChatSystem_GetNumInputDrivers },
    { "Sound_ChatSystem_GetInputDriverNameByIndex", Script_Sound_ChatSystem_GetInputDriverNameByIndex },
    { "Sound_ChatSystem_GetNumOutputDrivers", Script_Sound_ChatSystem_GetNumOutputDrivers },
    { "Sound_ChatSystem_GetOutputDriverNameByIndex", Script_Sound_ChatSystem_GetOutputDriverNameByIndex },
    { "Sound_GameSystem_GetNumInputDrivers", Script_Sound_GameSystem_GetNumInputDrivers },
    { "Sound_GameSystem_GetInputDriverNameByIndex", Script_Sound_GameSystem_GetInputDriverNameByIndex },
    { "Sound_GameSystem_GetNumOutputDrivers", Script_Sound_GameSystem_GetNumOutputDrivers },
    { "Sound_GameSystem_GetOutputDriverNameByIndex", Script_Sound_GameSystem_GetOutputDriverNameByIndex },
};
}
void SI2::RegisterSoundEngineCVars() {
    FrameScript::Native::RequireOwner();
    if (!CVar::m_initialized) throw std::logic_error("Sound preferences require the CVar owner");
    // Original defaults and saved values are preferences only. Backend-changing
    // callbacks are deliberately unselected until an audio owner exists.
    CVar::Register("Sound_OutputQuality", "sound quality, default 1 (medium)", 0, "1", nullptr, 7, false, nullptr, false);
    CVar::Register("Sound_NumChannels", "number of sound channels", 0, "32", nullptr, 7, false, nullptr, false);
    CVar::Register("Sound_EnableReverb", "", 0, "0", nullptr, 7, false, nullptr, false);
    CVar::Register("Sound_EnableSoftwareHRTF", "", 0, "0", nullptr, 7, false, nullptr, false);
    CVar::Register("Sound_OutputDriverIndex", "", 0, "0", nullptr, 7, false, nullptr, false);
    CVar::Register("Sound_OutputDriverName", "", 0, "Primary Sound Driver", nullptr, 7, false, nullptr, false);
    CVar::Register("Sound_DSPBufferSize", "sound buffer size, default 0", 0, "0", nullptr, 7, false, nullptr, false);
    CVar::Register("Sound_EnableHardware", "Enables Hardware", 0, "0", nullptr, 7, false, nullptr, false);
    CVar::Register("Sound_EnableMode2", "test", 0, "0", nullptr, 7, false, nullptr, false);
    CVar::Register("Sound_EnableMixMode2", "test", 0, "0", nullptr, 7, false, nullptr, false);
    CVar::Register("Sound_EnableSFX", "", 0, "1", nullptr, 7, false, nullptr, false);
    CVar::Register("Sound_EnableAmbience", "Enable Ambience", 0, "1", nullptr, 7, false, nullptr, false);
    CVar::Register("Sound_EnableErrorSpeech", "error speech", 0, "1", nullptr, 7, false, nullptr, false);
    CVar::Register("Sound_EnableMusic", "Enables music", 0, "1", nullptr, 7, false, nullptr, false);
    CVar::Register("Sound_EnableAllSound", "", 0, "1", nullptr, 7, false, nullptr, false);
    CVar::Register("Sound_MasterVolume", "master volume (0.0 to 1.0)", 0, "1.0", nullptr, 7, false, nullptr, false);
    CVar::Register("Sound_SFXVolume", "sound volume (0.0 to 1.0)", 0, "1.0", nullptr, 7, false, nullptr, false);
    CVar::Register("Sound_MusicVolume", "music volume (0.0 to 1.0)", 0, "0.4", nullptr, 7, false, nullptr, false);
    CVar::Register("Sound_AmbienceVolume", "Ambience Volume (0.0 to 1.0)", 0, "0.6", nullptr, 7, false, nullptr, false);
    CVar::Register("Sound_ListenerAtCharacter", "lock listener at character", 0, "1", nullptr, 7, false, nullptr, false);
    CVar::Register("Sound_EnableEmoteSounds", "", 0, "1", nullptr, 7, false, nullptr, false);
    CVar::Register("Sound_ZoneMusicNoDelay", "", 0, "0", nullptr, 7, false, nullptr, false);
    CVar::Register("Sound_EnableArmorFoleySoundForSelf", "", 0, "1", nullptr, 7, false, nullptr, false);
    CVar::Register("Sound_EnableArmorFoleySoundForOthers", "", 0, "1", nullptr, 7, false, nullptr, false);
    CVar::Register("Sound_EnableDSPEffects", "", 0, OsGetNumberOfProcessors() <= 1 ? "0" : "1", nullptr, 7, false, nullptr, false);
    CVar::Register("Sound_EnablePetSounds", "Enables pet sounds", 0, "1", nullptr, 7, false, nullptr, false);
    CVar::Register("Sound_EnableSoundWhenGameIsInBG", "Enable Sound When Game Is In Background", 0, "0", nullptr, 7, false, nullptr, false);
}
void SI2::RegisterScriptFunctions() {
    FrameScript::Native::RequireOwner();
    if (registered || !CVar::m_initialized) throw std::logic_error("Invalid sound query lifetime");
    RegisterSoundEngineCVars();
    for (const auto& function : functions) FrameScript_RegisterFunction(function.m_name, function.m_method);
    registered = true;
    std::cout << "native-unavailable=audio-backend" << std::endl;
}
void SI2::UnregisterScriptFunctions() {
    FrameScript::Native::RequireOwner();
    for (const auto& function : functions) FrameScript_UnregisterFunction(function.m_name);
    registered = false;
}
