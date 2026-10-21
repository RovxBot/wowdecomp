// Original 12340 FrameScript color conversion, shared with Windows model/UI callers.
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/Tempest/CImVector.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include <cmath>

// VERIFY: 1d94299e manual
// OFFSET: 0x00960420
int32_t FrameScript_GetColor(lua_State* L, int32_t index, CImVector& color) {
    float r = lua_tonumber(L, index);
    r = r < 0.0f ? 0.0f : (r >= 1.0f ? 1.0f : r);
    index++;

    float g = lua_tonumber(L, index);
    g = g < 0.0f ? 0.0f : (g >= 1.0f ? 1.0f : g);
    index++;

    float b = lua_tonumber(L, index);
    b = b < 0.0f ? 0.0f : (b >= 1.0f ? 1.0f : b);
    index++;

    float a;

    if (lua_isnumber(L, index)) {
        a = lua_tonumber(L, index);
        a = a < 0.0f ? 0.0f : (a >= 1.0f ? 1.0f : a);
        index++;
    } else {
        a = 1.0f;
    }

#ifdef WOWDECOMP_PORTABLE_RUNTIME
    if (!std::isfinite(r) || !std::isfinite(g) || !std::isfinite(b) || !std::isfinite(a))
        return luaL_error(L, "Color components must be finite");
#endif
    color.Set(a, r, g, b);
    return index;
}

// VERIFY: 51dc23ed manual
// OFFSET: 0x0061BE50
int32_t FrameScript_GetColorNoAlpha(lua_State* L, int32_t index, CImVector& color) {
    float r = lua_tonumber(L, index);
    r = r < 0.0f ? 0.0f : (r >= 1.0f ? 1.0f : r);
    index++;

    float g = lua_tonumber(L, index);
    g = g < 0.0f ? 0.0f : (g >= 1.0f ? 1.0f : g);
    index++;

    float b = lua_tonumber(L, index);
    b = b < 0.0f ? 0.0f : (b >= 1.0f ? 1.0f : b);
    index++;

#ifdef WOWDECOMP_PORTABLE_RUNTIME
    if (!std::isfinite(r) || !std::isfinite(g) || !std::isfinite(b))
        return luaL_error(L, "Color components must be finite");
#endif
    color.Set(1.0f, r, g, b);
    return index;
}

