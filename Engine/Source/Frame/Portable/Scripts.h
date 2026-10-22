#ifndef WOWDECOMP_NATIVE_FRAME_SCRIPTS_H
#define WOWDECOMP_NATIVE_FRAME_SCRIPTS_H
#include "Hierarchy.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include <exception>
#include <cmath>
#include <cfloat>
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
    return luaL_error(L, "This native Frame method requires an unavailable widget, XML, language or animation service");
}
template<int (*Method)(lua_State*)> int ScriptMethod(lua_State* L) {
    // Lua errors longjmp. Convert C++ failures only after their stack unwinds;
    // no owning automatic object may surround the subsequent luaL_error call.
    bool failed = false;
    try {
        RequireRegionScripts();
        RequireRoot();
        RequireTreeMutation();
        for (int i = 2; i <= lua_gettop(L); ++i)
            if (lua_isnumber(L, i) && (!std::isfinite(lua_tonumber(L, i)) ||
                std::fabs(lua_tonumber(L, i)) > FLT_MAX))
                return luaL_error(L, "Native Frame coordinates must fit finite floats");
        return Method(L);
    } catch (const std::exception&) { failed = true; }
    if (failed) return luaL_error(L, "Native Frame method rejected invalid state or arguments");
    return 0;
}
}}
#endif
