#include "Engine/Source/FrameScript/RetailUI/LuaCompat.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include "Engine/Source/lua-5.1/src/lstate.h"
#include <cstdlib>
#include <cstring>
#include <initializer_list>

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

// LuaTableExtensionsDocumentation: search all values with Lua equality, not
// just the consecutive array prefix (that is the distinct table.indexof API).
static int contains(lua_State* L) {
    if (!Enabled()) return luaL_error(L, "Forever Lua compatibility version is not enabled");
    luaL_checktype(L, 1, LUA_TTABLE);
    lua_settop(L, 2);
    lua_pushnil(L);
    while (lua_next(L, 1)) {
        if (lua_equal(L, -1, 2)) {
            lua_pushboolean(L, true);
            return 1;
        }
        lua_pop(L, 1);
    }
    lua_pushboolean(L, false);
    return 1;
}

// The 12340 VM has no separate secure-delegate authority/security context:
// lua_tainted also controls protected actions. Never clear an insecure caller
// to implement the modern API. This bounded prerequisite admits trusted entry
// only; unsupported cross-taint calls fail before invoking the Lua function.
static bool tainted(const TValue* value) {
    return value->taint || (iscollectable(value) && gcvalue(value)->gch.taint);
}

static void trustedContext(lua_State* L) {
    if (lua_tainted || lua_taintedclosure || lua_closuretaint)
        luaL_error(L, "CreateSecureDelegate: tainted security contexts are unsupported by the 12340 VM");
    // The interpreter resets lua_taintedclosure before entering C functions.
    // Inspect live callers as well, including direct host calls with taint
    // propagation disabled. This does not reconstruct erased tail-call history.
    for (const CallInfo* caller = L->ci; caller > L->base_ci; --caller) {
        if (ttisfunction(caller->func) && tainted(caller->func))
            luaL_error(L, "CreateSecureDelegate: tainted caller closures are unsupported");
    }
}

struct TaintState {
    const char* execution;
    const char* closure;
    const char* gc;
    int expected;
    int activeClosure;
};

static int invokeDelegate(lua_State* L) {
    if (!Enabled()) return luaL_error(L, "Forever Lua compatibility version is not enabled");
    trustedContext(L);
    const TValue* function = &curr_func(L)->c.upvalue[0];
    // setfenv can change a retained Lua closure's taint after creation. Check
    // its live ownership on every call; never clone or untaint that closure.
    if (tainted(function))
        return luaL_error(L, "CreateSecureDelegate: tainted callees are unsupported");
    const int arguments = lua_gettop(L);
    for (int i = 0; i < arguments; ++i) {
        if (tainted(L->base + i))
            return luaL_error(L, "CreateSecureDelegate: tainted arguments and untrusted function wrapping are unsupported");
    }
    if (!lua_checkstack(L, 1)) return luaL_error(L, "CreateSecureDelegate: insufficient stack");
    lua_pushvalue(L, lua_upvalueindex(1));
    lua_insert(L, 1);
    const TaintState saved{lua_tainted, lua_closuretaint, lua_gctag,
                           lua_taintexpected, lua_taintedclosure};
    // Admit normal VM taint propagation within the protected call. Merely
    // reading an addon-owned value must still make protected actions insecure.
    // No C++ owner/destructor crosses Lua's longjmp boundary.
    if (!lua_taintexpected) lua_taintexpected = 1;
    const int status = lua_pcall(L, arguments, LUA_MULTRET, 0);
    lua_tainted = saved.execution;
    lua_closuretaint = saved.closure;
    lua_gctag = saved.gc;
    lua_taintexpected = saved.expected;
    lua_taintedclosure = saved.activeClosure;
    // Preserve the exact error object and the VM's stack/upvalue unwinding.
    // This boundary does not swallow failures or call the global error handler.
    if (status) return lua_error(L);
    return lua_gettop(L);
}

static int createSecureDelegate(lua_State* L) {
    if (!Enabled()) return luaL_error(L, "Forever Lua compatibility version is not enabled");
    trustedContext(L);
    luaL_checktype(L, 1, LUA_TFUNCTION);
    if (lua_iscfunction(L, 1)) return luaL_argerror(L, 1, "a Lua function is required");
    if (tainted(L->base)) return luaL_argerror(L, 1, "tainted callees are unsupported");
    if (lua_gettop(L) > 2) return luaL_error(L, "CreateSecureDelegate: expected function and optional default options");
    if (!lua_isnoneornil(L, 2)) {
        luaL_checktype(L, 2, LUA_TTABLE);
        if (tainted(L->base + 1)) return luaL_argerror(L, 2, "tainted options are unsupported");
        if (lua_getmetatable(L, 2)) return luaL_argerror(L, 2, "options metatables are unsupported");
        lua_pushnil(L);
        while (lua_next(L, 2)) {
            if (tainted(L->top - 2) || tainted(L->top - 1))
                return luaL_argerror(L, 2, "tainted option fields are unsupported");
            size_t length = 0;
            const char* key = lua_type(L, -2) == LUA_TSTRING ? lua_tolstring(L, -2, &length) : nullptr;
            if (!key || length != sizeof("wrapUntrustedFunctions") - 1 ||
                std::memcmp(key, "wrapUntrustedFunctions", length))
                return luaL_argerror(L, 2, "unknown secure delegate option (secret/security-context options unsupported)");
            if (!lua_isboolean(L, -1) || !lua_toboolean(L, -1))
                return luaL_argerror(L, 2, "only default wrapUntrustedFunctions=true is supported");
            lua_pop(L, 1);
        }
        trustedContext(L);
    }
    // One GC-traced upvalue owns the original closure and its captures. No
    // registry roots, external taint pointers or per-VM native owners remain.
    lua_pushvalue(L, 1);
    lua_pushcclosure(L, invokeDelegate, 1);
    return 1;
}

bool IsHookRestrictedFunction(lua_State* L, int index) {
    const lua_CFunction function = lua_tocfunction(L, index);
    // Factory restriction is declared by the pinned API. Generated-delegate
    // hooks are conservatively refused locally until independently qualified.
    return function == mixin || function == createFromMixins ||
           function == createSecureDelegate || function == invokeDelegate;
}

// Selected 70862 LFGConstantsDocumentation.lua (reference lock). These are
// presentation identifiers, NOT the 12340 leader/tank/healer/damage bitmask.
// Keep ordinary Lua tables/lookup/errors; no metatable manufactures values.
static char constantsOwner;
struct Constant { const char* name; int value; };
static const Constant roles[] = {{"Tank", 0}, {"Healer", 1}, {"Damage", 2}};
static const Constant roleMeta[] = {{"NumValues", 3}, {"MinValue", 0}, {"MaxValue", 2}};
static const Constant roleConstants[] = {{"LFG_ROLE_NO_ROLE", -1}, {"LFG_ROLE_ANY", 3}};
struct ConstantTable { const char* root; const char* name; const Constant* fields; int count; };
static const ConstantTable constantTables[] = {
    {"Enum", "LFGRole", roles, 3}, {"Enum", "LFGRoleMeta", roleMeta, 3},
    {"Constants", "LFG_ROLEConstants", roleConstants, 2},
};

int RegisterConstants(lua_State* L) {
    if (!Enabled()) return luaL_error(L, "Forever constants version is not enabled");
    const int top = lua_gettop(L);
    lua_pushlightuserdata(L, &constantsOwner);
    lua_rawget(L, LUA_REGISTRYINDEX);
    const bool existing = !lua_isnil(L, -1);
    const int owner = top + 1;
    if (!existing) {
        lua_pop(L, 1);
        lua_newtable(L);
    }
    // Preflight both namespaces before publishing either one. Never adopt an
    // addon-owned table, run its metamethods, or overwrite a conflicting root.
    for (const char* root : {"Enum", "Constants"}) {
        lua_pushstring(L, root); lua_rawget(L, LUA_GLOBALSINDEX);
        if (existing) {
            lua_getfield(L, owner, root);
            if (!lua_rawequal(L, -1, -2)) return luaL_error(L, "Forever constants: foreign %s namespace", root);
            lua_pop(L, 1);
        } else if (!lua_isnil(L, -1)) return luaL_error(L, "Forever constants: conflicting %s namespace", root);
        lua_pop(L, 1);
    }
    for (const auto& table : constantTables) {
        if (existing) {
            lua_getfield(L, owner, table.root);
            if (lua_getmetatable(L, -1)) return luaL_error(L, "Forever constants: namespace metatable");
            lua_pushstring(L, table.name); lua_rawget(L, -2);
            lua_getfield(L, owner, table.name);
            if (!lua_rawequal(L, -1, -2)) return luaL_error(L, "Forever constants: foreign %s table", table.name);
            lua_pop(L, 1);
            if (lua_getmetatable(L, -1)) return luaL_error(L, "Forever constants: enum metatable");
            int count = 0;
            lua_pushnil(L);
            while (lua_next(L, -2)) { ++count; lua_pop(L, 1); }
            if (count != table.count) return luaL_error(L, "Forever constants: invalid %s membership", table.name);
            for (int i = 0; i < table.count; ++i) {
                lua_pushstring(L, table.fields[i].name); lua_rawget(L, -2);
                if (lua_type(L, -1) != LUA_TNUMBER || lua_tonumber(L, -1) != table.fields[i].value)
                    return luaL_error(L, "Forever constants: invalid %s.%s", table.name, table.fields[i].name);
                lua_pop(L, 1);
            }
            lua_pop(L, 2);
        } else {
            lua_getfield(L, owner, table.root);
            if (lua_isnil(L, -1)) { lua_pop(L, 1); lua_newtable(L); lua_pushvalue(L, -1); lua_setfield(L, owner, table.root); }
            lua_newtable(L);
            for (int i = 0; i < table.count; ++i) {
                lua_pushinteger(L, table.fields[i].value); lua_setfield(L, -2, table.fields[i].name);
            }
            lua_pushvalue(L, -1); lua_setfield(L, owner, table.name);
            lua_setfield(L, -2, table.name);
            lua_pop(L, 1);
        }
    }
    if (!existing) {
        for (const char* root : {"Enum", "Constants"}) {
            lua_pushstring(L, root); lua_getfield(L, owner, root); lua_rawset(L, LUA_GLOBALSINDEX);
        }
        lua_pushlightuserdata(L, &constantsOwner); lua_pushvalue(L, owner); lua_rawset(L, LUA_REGISTRYINDEX);
    }
    lua_settop(L, top);
    return 0;
}

void RegisterLuaFunctions(lua_State* L) {
    if (!Enabled()) return;
    RegisterConstants(L);
    lua_pushcfunction(L, mixin);
    lua_setglobal(L, "Mixin");
    lua_pushcfunction(L, createFromMixins);
    lua_setglobal(L, "CreateFromMixins");
    lua_pushcfunction(L, createSecureDelegate);
    lua_setglobal(L, "CreateSecureDelegate");
    lua_getglobal(L, "table");
    lua_pushcfunction(L, contains);
    lua_setfield(L, -2, "contains");
    lua_pop(L, 1);
}
}
