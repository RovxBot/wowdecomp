#ifndef WOWDECOMP_NATIVE_FRAME_SCRIPTS_H
#define WOWDECOMP_NATIVE_FRAME_SCRIPTS_H
#include "Hierarchy.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include <exception>
#include <cmath>
#include <cfloat>
#include <cstdio>
namespace Frame { namespace Native {
// Install after Fonts; close every region/script object before this owner.
class Scripts final {
public:
    Scripts();
    ~Scripts();
    Scripts(const Scripts&) = delete;
    Scripts& operator=(const Scripts&) = delete;
    void CheckCanClose() const;
};
void RequireRegionScripts();
inline int UnavailableScriptMethod(lua_State* L) {
    lua_Debug call{};
    const char* method = lua_getstack(L, 0, &call) && lua_getinfo(L, "n", &call) && call.name ? call.name : "<unknown>";
    return luaL_error(L, "Native Frame method '%s' requires an unavailable widget, XML, language or animation service", method);
}
template<int (*Method)(lua_State*), bool Coordinates = true, bool TreeMutation = true, bool StateMutation = false> int ScriptMethod(lua_State* L) {
    // Lua errors longjmp. Convert C++ failures only after their stack unwinds;
    // no owning automatic object may surround the subsequent luaL_error call.
    char failure[512]{};
    try {
        RequireRegionScripts();
        RequireRoot();
        if (TreeMutation) RequireTreeMutation();
        else if (StateMutation) RequireStateMutation();
        for (int i = 2; i <= lua_gettop(L); ++i)
            if (Coordinates && lua_isnumber(L, i) && (!std::isfinite(lua_tonumber(L, i)) ||
                std::fabs(lua_tonumber(L, i)) > FLT_MAX))
                return luaL_error(L, "Native Frame coordinates must fit finite floats");
        return Method(L);
    } catch (const std::exception& error) {
        std::snprintf(failure, sizeof(failure), "%s", error.what());
    }
    if (*failure) return luaL_error(L, "Native Frame method rejected invalid state or arguments: %s", failure);
    return luaL_error(L, "Native Frame method rejected invalid state or arguments");
}
}}
#endif
