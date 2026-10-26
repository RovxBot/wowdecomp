// Selected original 12340 VideoOptions_Scripts queries through the installed
// native Gx owner. These query option state/capabilities, not successful option
// application. Unselected graphics setters/restart remain visibly unavailable.
#include "WoW/Source/Ui/VideoOptions.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/FrameScript/FrameScript_Object.h"
#include "Engine/Source/FrameScript/Portable/Runtime.h"
#include "Engine/Source/ConsoleBase/ConsoleVar.h"
#include "Engine/Source/ConsoleBase/Types.h"
#include "Engine/Source/Gx/CGxDevice/CGxDevice.h"
#include "Engine/Source/Gx/CGxFormat.h"
#include "Engine/Source/Gx/Gx.h"
#include "Engine/Source/Os/System.h"
#include "Engine/Source/Tempest/C2iVector.h"
#include "Engine/Source/Tempest/C3iVector.h"
#include "Engine/Source/Storm/H/storm.h"
#include <alloca.h>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <stdexcept>
#include <iostream>
namespace {
bool registered = false;
TSGrowableArray<C2iVector> s_resolutions;
TSGrowableArray<C3iVector> s_multisampleFormats;
const int32_t s_formatBits[CGxFormat::Formats_Last] = {16,24,24,30,16,24,24,32};
void RequireVideo(lua_State* L) {
    FrameScript::Native::RequireOwner();
    if (!registered || !g_theGxDevicePtr) luaL_error(L, "Native video-option queries are closed");
    g_theGxDevicePtr->DeviceWindow(); // Existing main-thread/current-context check.
}
// SetupMultisampleFormats uses the qualified drawable format only. No extra
// sample count, display mode, stereo or graphics capability is inferred.
void SetupMultisampleFormats() {
    if (s_multisampleFormats.Count()) return;
    const auto& active = g_theGxDevicePtr->m_format;
    C3iVector format;
    format.x = s_formatBits[active.m_colorFormat];
    format.y = s_formatBits[active.m_depthFormat];
    format.z = active.m_multisampleCount;
    s_multisampleFormats.New(format);
}
static int CompareRefreshRates(const void* a, const void* b) {
    int32_t left = *static_cast<const int32_t*>(a);
    int32_t right = *static_cast<const int32_t*>(b);

    if (left < right) {
        return -1;
    }

    return right < left;
}

static void SetupFormats() {
    if (s_resolutions.Count()) {
        return;
    }

    bool useDefaults = false;
    CVar* widescreen = CVar::Lookup("widescreen");

    if (widescreen && widescreen->m_intValue) {
        TSGrowableArray<CGxMonitorMode> modes;
        GxAdapterMonitorModes(modes);

        C2iVector last = { 0, 0 };

        for (uint32_t i = 0; i < modes.Count(); i++) {
            const C2iVector& size = modes[i].m_size;

            if (static_cast<float>(size.x) / size.y >= 1.248f && size.x > 640 && size.y > 480 && (size.x != last.x || size.y != last.y)) {
                last = size;
                s_resolutions.New(last);
            }
        }

        useDefaults = s_resolutions.Count() == 0;
    } else {
        useDefaults = true;
    }

    if (!useDefaults) {
        return;
    }

    C2iVector resolution;
    resolution.x = 800;
    resolution.y = 600;
    s_resolutions.New(resolution);
    resolution.x = 1024;
    resolution.y = 768;
    s_resolutions.New(resolution);
    resolution.x = 1280;
    resolution.y = 1024;
    s_resolutions.New(resolution);
    resolution.x = 1600;
    resolution.y = 1200;
    s_resolutions.New(resolution);
}

static int32_t Script_GetVideoCaps(lua_State* L) {
    RequireVideo(L);
    CGxCaps& caps = g_theGxDevicePtr->Caps();

    if (caps.m_texFilterAnisotropic) {
        lua_pushnumber(L, 1.0);
    } else {
        lua_pushnil(L);
    }

    if (caps.m_shaderTargets[GxSh_Pixel]) {
        lua_pushnumber(L, 1.0);
    } else {
        lua_pushnil(L);
    }

    if (caps.m_shaderTargets[GxSh_Vertex]) {
        lua_pushnumber(L, 1.0);
    } else {
        lua_pushnil(L);
    }

    if (caps.m_texFilterTrilinear) {
        lua_pushnumber(L, 1.0);
    } else {
        lua_pushnil(L);
    }

    lua_pushnumber(L, caps.unk_0018);

    if (caps.m_maxTexAnisotropy) {
        lua_pushnumber(L, caps.m_maxTexAnisotropy);
    } else {
        lua_pushnil(L);
    }

    if (caps.m_hwCursor) {
        lua_pushnumber(L, 1.0);
    } else {
        lua_pushnil(L);
    }

    return 7;
}

static int32_t Script_IsStereoVideoAvailable(lua_State* L) {
    RequireVideo(L);
    if (g_theGxDevicePtr->Caps().m_stereoAvailable) {
        lua_pushnumber(L, 1.0);
    } else {
        lua_pushnil(L);
    }

    return 1;
}

static int32_t Script_IsPlayerResolutionAvailable(lua_State* L) {
    RequireVideo(L);
    if (OsGetNumberOfProcessors() <= 1) {
        lua_pushnil(L);
    } else {
        lua_pushnumber(L, 1.0);
    }

    return 1;
}

static int32_t Script_GetScreenResolutions(lua_State* L) {
    RequireVideo(L);
    SetupFormats();
    lua_checkstack(L, s_resolutions.Count());

    for (uint32_t i = 0; i < s_resolutions.Count(); i++) {
        char resolution[64];
        SStrPrintf(resolution, sizeof(resolution), "%dx%d", s_resolutions[i].x, s_resolutions[i].y);
        lua_pushstring(L, resolution);
    }

    return s_resolutions.Count();
}

static int32_t Script_GetCurrentResolution(lua_State* L) {
    RequireVideo(L);
    SetupFormats();

    CVar* gxResolution = CVar::Lookup("gxResolution");

    if (gxResolution && gxResolution->m_stringValue.m_str) {
        int32_t width = 0;
        int32_t height = 0;
        char separator;
        sscanf(gxResolution->m_stringValue.m_str, "%d%c%d", &width, &separator, &height);

        for (uint32_t i = 0; i < s_resolutions.Count(); i++) {
            if (width == s_resolutions[i].x && height == s_resolutions[i].y) {
                lua_pushnumber(L, i + 1);
                return 1;
            }
        }
    }

    lua_pushnumber(L, 1.0);
    return 1;
}

static int32_t Script_GetMultisampleFormats(lua_State* L) {
    RequireVideo(L);
    SetupMultisampleFormats();
    lua_checkstack(L, 3 * s_multisampleFormats.Count());

    for (uint32_t i = 0; i < s_multisampleFormats.Count(); i++) {
        lua_pushnumber(L, s_multisampleFormats[i].x);
        lua_pushnumber(L, s_multisampleFormats[i].y);
        lua_pushnumber(L, s_multisampleFormats[i].z);
    }

    return 3 * s_multisampleFormats.Count();
}

static int32_t Script_GetCurrentMultisampleFormat(lua_State* L) {
    RequireVideo(L);
    SetupMultisampleFormats();

    int32_t colorBits = 0;
    int32_t depthBits = 0;
    int32_t multisample = 0;

    CVar* var = CVar::Lookup("gxColorBits");

    if (var) {
        colorBits = var->m_intValue;
    }

    var = CVar::Lookup("gxDepthBits");

    if (var) {
        depthBits = var->m_intValue;
    }

    var = CVar::Lookup("gxMultisample");

    if (var) {
        multisample = var->m_intValue;
    }

    for (uint32_t i = 0; i < s_multisampleFormats.Count(); i++) {
        if (colorBits == s_multisampleFormats[i].x && depthBits == s_multisampleFormats[i].y && multisample == s_multisampleFormats[i].z) {
            lua_pushnumber(L, i + 1);
            return 1;
        }
    }

    lua_pushnumber(L, 1.0);
    return 1;
}

static int32_t Script_GetRefreshRates(lua_State* L) {
    RequireVideo(L);
    SetupFormats();

    uint32_t index = 0;

    if (lua_isnumber(L, 1)) {
        const auto value = lua_tonumber(L, 1);
        if (!std::isfinite(value) || value < 1 || value > s_resolutions.Count()) return 0;
        uint32_t count = s_resolutions.Count();
        index = count < static_cast<uint32_t>(lua_tonumber(L, 1)) - 1 ? s_resolutions.Count() : static_cast<uint32_t>(lua_tonumber(L, 1)) - 1;
    }

    if (index >= s_resolutions.Count()) {
        return 0;
    }

    TSGrowableArray<CGxMonitorMode> modes;
    GxAdapterMonitorModes(modes);

    int32_t* rates = static_cast<int32_t*>(alloca(sizeof(int32_t) * modes.Count()));
    uint32_t numRates = 0;

    for (uint32_t i = 0; i < modes.Count(); i++) {
        if (s_resolutions[index].x == modes[i].m_size.x && s_resolutions[index].y == modes[i].m_size.y) {
            rates[numRates++] = modes[i].m_refreshRate;
        }
    }

    if (numRates) {
        qsort(rates, numRates, sizeof(int32_t), CompareRefreshRates);
    }

    int32_t results = 0;

    for (uint32_t i = 0; i < numRates; i++) {
        if (i && rates[i] == rates[i - 1]) {
            continue;
        }

        lua_pushnumber(L, rates[i]);
        results++;
    }

    return results;
}
static int32_t Script_GetGamma(lua_State* L) {
    RequireVideo(L);
    CVar* gamma = CVar::Lookup("gamma");
    lua_pushnumber(L, 1.0 - gamma->m_floatValue);
    return 1;
}

// Original SetGamma (0x0054EA90) maps the slider to 1 - value. A refresh
// of the existing preference needs no device mutation. Reject changes until
// native display-ramp ownership is implemented; storing a value is not applying it.
static int32_t Script_SetGamma(lua_State* L) {
    RequireVideo(L);
    if (!lua_isnumber(L, 1)) return luaL_error(L, "Usage: SetGamma(value)");
    const double gamma = 1.0 - lua_tonumber(L, 1);
    if (!std::isfinite(gamma)) return luaL_error(L, "Gamma must be finite");
    auto* current = CVar::Lookup("gamma");
    if (gamma != current->m_floatValue)
        return luaL_error(L, "Native display gamma application unavailable");
    return 0;
}

static int32_t Script_GetTerrainMip(lua_State* L) {
    RequireVideo(L);
    CVar* shadowLevel = CVar::Lookup("shadowLevel");
    lua_pushnumber(L, 1.0 - shadowLevel->m_intValue);
    return 1;
}
// Original SetTerrainMip (0x0054EB40), with checked native conversion. As with
// gamma, activation may refresh the current preference, but cannot apply a new
// terrain setting without the world renderer.
static int32_t Script_SetTerrainMip(lua_State* L) {
    RequireVideo(L);
    if (!lua_isnumber(L, 1)) return luaL_error(L, "Usage: SetTerrainMip(value)");
    const double value = lua_tonumber(L, 1);
    if (!std::isfinite(value) || value < 1.0 - INT32_MAX || value > INT32_MAX)
        return luaL_error(L, "Terrain mip is outside the original integer range");
    if (1 - static_cast<int32_t>(value) != CVar::Lookup("shadowLevel")->m_intValue)
        return luaL_error(L, "Native terrain mip application unavailable");
    return 0;
}
FrameScript_Method functions[] = {
    { "GetGamma", Script_GetGamma },
    { "SetGamma", Script_SetGamma },
    { "GetTerrainMip", Script_GetTerrainMip },
    { "SetTerrainMip", Script_SetTerrainMip },
    { "GetVideoCaps", Script_GetVideoCaps },
    { "IsStereoVideoAvailable", Script_IsStereoVideoAvailable },
    { "IsPlayerResolutionAvailable", Script_IsPlayerResolutionAvailable },
    { "GetScreenResolutions", Script_GetScreenResolutions },
    { "GetCurrentResolution", Script_GetCurrentResolution },
    { "GetMultisampleFormats", Script_GetMultisampleFormats },
    { "GetCurrentMultisampleFormat", Script_GetCurrentMultisampleFormat },
    { "GetRefreshRates", Script_GetRefreshRates },
 };
}
void CGVideoOptions::RegisterScriptFunctions() {
    FrameScript::Native::RequireOwner();
    if (registered || !g_theGxDevicePtr || !CVar::m_initialized) throw std::logic_error("Native video queries require live CVar/Gx owners");
    g_theGxDevicePtr->DeviceWindow();
    // ConsoleDevice_CVar.cpp registers these option values from the chosen
    // original format. Native startup already selected this actual RGBA8/D24S8
    // single-sample drawable; do not advertise other formats or a renderer.
    const auto& active = g_theGxDevicePtr->m_format;
    if (active.m_colorFormat >= CGxFormat::Formats_Last || active.m_depthFormat >= CGxFormat::Formats_Last || active.m_multisampleCount > INT32_MAX)
        throw std::logic_error("Invalid native drawable format");
    char value[64];
    std::snprintf(value,sizeof(value),"%d",s_formatBits[active.m_colorFormat]);
    CVar::Register("gxColorBits","color bits",3,value,nullptr,CATEGORY_GRAPHICS,false,nullptr,false);
    std::snprintf(value,sizeof(value),"%d",s_formatBits[active.m_depthFormat]);
    CVar::Register("gxDepthBits","depth bits",3,value,nullptr,CATEGORY_GRAPHICS,false,nullptr,false);
    std::snprintf(value,sizeof(value),"%u",active.m_multisampleCount);
    CVar::Register("gxMultisample","multisample count",3,value,nullptr,CATEGORY_GRAPHICS,false,nullptr,false);
    std::snprintf(value,sizeof(value),"%dx%d",active.m_size.x,active.m_size.y);
    CVar::Register("gxResolution","resolution",3,value,nullptr,CATEGORY_GRAPHICS,false,nullptr,false);
    CVar::Register("gxRefresh","refresh rate",3,"75",nullptr,CATEGORY_GRAPHICS,false,nullptr,false);
    CVar::Register("gxApi","graphics api",3,"OpenGL",nullptr,CATEGORY_GRAPHICS,false,nullptr,false);
    CVar::Register("widescreen","Allow widescreen support",0,"1",nullptr,CATEGORY_GRAPHICS,false,nullptr,false);
    // Original graphics/world/component preferences from ConsoleDevice_CVar,
    // Client_CVar, WorldParam_CVar, Client, MapWeather and CharacterComponent.
    // No backend-changing callbacks or world objects are fabricated. These
    // values remain saved preferences until those owners are integrated.
    CVar::Register("gxStereoEnabled", "Enable stereoscopic rendering", 1, "0", nullptr, CATEGORY_GRAPHICS, false, nullptr, false);
    CVar::Register("gxStereoConvergence", "Set stereoscopic rendering convergence depth", 1, "1", nullptr, CATEGORY_GRAPHICS, false, nullptr, false);
    CVar::Register("gxStereoSeparation", "Set stereoscopic rendering separation percentage", 1, "25", nullptr, CATEGORY_GRAPHICS, false, nullptr, false);
    CVar::Register("gxVSync", "vsync on or off", 3, "1", nullptr, CATEGORY_GRAPHICS, false, nullptr, false);
    CVar::Register("gxTripleBuffer", "triple buffer", 3, "0", nullptr, CATEGORY_GRAPHICS, false, nullptr, false);
    CVar::Register("gxCursor", "toggle hardware cursor", 3, "1", nullptr, CATEGORY_GRAPHICS, false, nullptr, false);
    CVar::Register("windowResizeLock", "prevent resizing in windowed mode", 1, "0", nullptr, CATEGORY_GRAPHICS, false, nullptr, false);
    CVar::Register("DesktopGamma", nullptr, 0, "0", nullptr, CATEGORY_GRAPHICS, false, nullptr, false);
    CVar::Register("Gamma", nullptr, 0, "1.0", nullptr, CATEGORY_GRAPHICS, false, nullptr, false);
    CVar::Register("farclip", "Far clip plane distance", 1, "350", nullptr, CATEGORY_GRAPHICS, false, nullptr, false);
    CVar::Register("particleDensity", "Video option: Particle density", 1, "1.0", nullptr, CATEGORY_GRAPHICS, false, nullptr, false);
    CVar::Register("environmentDetail", "Environment detail", 1, "1.0", nullptr, CATEGORY_GRAPHICS, false, nullptr, false);
    CVar::Register("groundEffectDensity", "Ground effect density", 1, "16", nullptr, CATEGORY_GRAPHICS, false, nullptr, false);
    CVar::Register("groundEffectDist", "Ground effect dist", 1, "70.0", nullptr, CATEGORY_GRAPHICS, false, nullptr, false);
    CVar::Register("baseMip", "base mipmap level", 1, "0", nullptr, CATEGORY_GRAPHICS, false, nullptr, false);
    CVar::Register("shadowLevel", "Terrain shadow map mip level", 1, "1", nullptr, CATEGORY_GRAPHICS, false, nullptr, false);
    CVar::Register("extShadowQuality", "Quality of exterior shadows (0-5)", 1, "0", nullptr, CATEGORY_GRAPHICS, false, nullptr, false);
    CVar::Register("textureFilteringMode", "Texture filtering mode", 1, "1", nullptr, CATEGORY_GRAPHICS, false, nullptr, false);
    CVar::Register("weatherDensity", nullptr, 0, "2", nullptr, CATEGORY_DEFAULT, false, nullptr, false);
    CVar::Register("componentTextureLevel", "Number of mip levels used for character component textures", 0x1, "8", nullptr, 0, false, nullptr, false);
    CVar::Register("specular", "Specularity", 1, "0", nullptr, CATEGORY_GRAPHICS, false, nullptr, false);
    CVar::Register("projectedTextures", "Projected Textures", 1, "0", nullptr, CATEGORY_GRAPHICS, false, nullptr, false);
    CVar::Register("gxWindow","toggle fullscreen/window",3,active.m_window ? "1" : "0",nullptr,CATEGORY_GRAPHICS,false,nullptr,false);
    CVar::Register("gxMaximize","maximize game window",3,active.m_maximize ? "1" : "0",nullptr,CATEGORY_GRAPHICS,false,nullptr,false);
    CVar::Register("gxFixLag","prevent cursor lag",3,active.m_fixLag ? "1" : "0",nullptr,CATEGORY_GRAPHICS,false,nullptr,false);
    // EffectDeath's original base Effect defaults enabled; no death renderer is selected.
    CVar::Register("ffxDeath","full screen death effect",1,"1",nullptr,CATEGORY_GRAPHICS,false,nullptr,false);
    for (const auto& function : functions) FrameScript_RegisterFunction(function.m_name,function.m_method);
    registered = true;
    std::cout << "native-unavailable=display-gamma,graphics-option-application" << std::endl;
}
void CGVideoOptions::UnregisterScriptFunctions() {
    FrameScript::Native::RequireOwner();
    for (const auto& function : functions) FrameScript_UnregisterFunction(function.m_name);
    registered = false;
    s_resolutions.Clear(); s_multisampleFormats.Clear();
}
