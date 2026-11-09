#include "Engine/Source/FrameScript/RetailUI/LuaCompat.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <cstdlib>
#include <cstring>

namespace RovRetailUI {
bool Enabled() {
    const char* value = std::getenv("WOWDECOMP_RETAIL_UI_COMPAT");
    return value && !std::strcmp(value, Version);
}

// The pinned FrameScriptDocumentation declares native composition APIs;
// SharedXMLBase/Mixin.lua captures them and supplies the Lua SecureMixin loop.
// Implement ordinary shallow composition using the existing VM's assignment,
// error, GC barrier and taint paths. No secure delegate/secret-value semantics
// are implied by copying a function reference.
static int compose(lua_State* L, bool create) {
    if (!Enabled()) return luaL_error(L, "Forever Lua compatibility version is not enabled");
    const int arguments = lua_gettop(L);
    if (create) {
        lua_newtable(L);
        lua_insert(L, 1);
    } else {
        luaL_checktype(L, 1, LUA_TTABLE);
    }
    const int last = arguments + (create ? 1 : 0);
    lua_rawgeti(L, 1, 0);
    const bool nativeTarget = lua_islightuserdata(L, -1);
    lua_pop(L, 1);
    for (int source = 2; source <= last; ++source) {
        luaL_checktype(L, source, LUA_TTABLE);
        // A frame's native pointer is not a transferable Lua mixin field. In
        // particular CreateFromMixins(frame) must never create a second owner.
        lua_rawgeti(L, source, 0);
        const bool nativeSource = lua_islightuserdata(L, -1);
        lua_pop(L, 1);
        if (nativeSource) return luaL_error(L, "Mixin cannot copy a native object's identity");
        lua_pushnil(L);
        while (lua_next(L, source)) {
            if (nativeTarget && lua_type(L, -2) == LUA_TNUMBER && lua_tonumber(L, -2) == 0)
                return luaL_error(L, "Mixin cannot replace a native object's identity");
            lua_pushvalue(L, -2);
            lua_pushvalue(L, -2);
            lua_settable(L, 1);
            lua_pop(L, 1);
        }
    }
    lua_pushvalue(L, 1);
    return 1;
}

static int mixin(lua_State* L) { return compose(L, false); }
static int createFromMixins(lua_State* L) { return compose(L, true); }

bool IsCompositionFunction(lua_State* L, int index) {
    const lua_CFunction function = lua_tocfunction(L, index);
    return function == mixin || function == createFromMixins;
}

void RegisterLuaFunctions(lua_State* L) {
    if (!Enabled()) return;
    lua_pushcfunction(L, mixin);
    lua_setglobal(L, "Mixin");
    lua_pushcfunction(L, createFromMixins);
    lua_setglobal(L, "CreateFromMixins");
}
}
