// Authored fixtures use the actual full-client Lua, XML nodes and compatibility
// implementation. Optional local reference execution never packages input files.
#include "Engine/Source/Frame/RetailUI/XMLCompat.h"
#include "Engine/Source/Base/Status.h"
#include "Engine/Source/Base/RCString.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/FrameXML/XMLTree.h"
#include "Engine/Source/FrameXML/FrameXML.h"
#include "Engine/Source/Frame/RetailUI/FontResources.h"
#include "Engine/Source/Frame/RetailUI/Runtime.h"
#include "Engine/Source/Frame/CSimpleFont.h"
#include "Engine/Source/BlizzardCore/SFile2-Core.h"
#include "Engine/Source/Gxu/GxuFont.h"
#include "Engine/Source/Gxu/GxuFontFace.h"
#include "Engine/Source/Gxu/GxuFontUtil.h"
#include "Engine/Source/Gxu/Slug.h"
#include "Engine/Source/Gxu/GxuFontFont.h"
#include "Engine/Source/Services/TextBlock.h"
#include <freetype/ftmodule.h>
#include "Engine/Source/Base/Coordinate.h"
#include "Engine/Source/Os/TimeManager.h"
#include "Engine/Source/Storm/H/storm.h"
#include "Engine/Source/Storm/Source/MD5.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include "Engine/Source/lua-5.1/src/lstate.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
#include <cmath>
#include <filesystem>

static void check(bool ok, int line) {
    if (!ok) { std::fprintf(stderr, "retail XML check failed at %d\n", line); std::abort(); }
}
#define require(x) check(bool(x), __LINE__)

static void run(lua_State* L, const char* code) {
    if (luaL_loadstring(L, code) || lua_pcall(L, 0, 0, 0)) {
        std::fprintf(stderr, "Lua: %s\n", lua_tostring(L, -1));
        std::abort();
    }
}

static void constantsFixture(lua_State* L) {
    lua_pushcfunction(L, RovRetailUI::RegisterConstants);
    lua_setglobal(L, "checkNativeConstants");
    run(L, R"lua(
        assert(Enum.LFGRole.Tank == 0 and Enum.LFGRole.Healer == 1 and Enum.LFGRole.Damage == 2);
        assert(Constants.LFG_ROLEConstants.LFG_ROLE_NO_ROLE == -1 and Constants.LFG_ROLEConstants.LFG_ROLE_ANY == 3);
        assert(Enum.LFGRole.Unknown == nil and Enum.Unknown == nil and Constants.Unknown == nil);
        assert(not pcall(function() return Enum.Unknown.Value end));
        local roles, enums, constants = Enum.LFGRole, Enum, Constants;
        checkNativeConstants(); assert(Enum == enums and Enum.LFGRole == roles);
        Enum.LFGRole.Tank = '0'; assert(not pcall(checkNativeConstants)); Enum.LFGRole.Tank = 0;
        Enum.LFGRole.Tank = nil; assert(not pcall(checkNativeConstants)); Enum.LFGRole.Tank = 0;
        Enum.LFGRole.Extra = 3; assert(not pcall(checkNativeConstants)); Enum.LFGRole.Extra = nil;
        Enum.LFGRole = {Tank = 0, Healer = 1, Damage = 2}; assert(not pcall(checkNativeConstants)); Enum.LFGRole = roles;
        setmetatable(roles, {__index = function() error('foreign lookup') end});
        assert(not pcall(checkNativeConstants)); setmetatable(roles, nil);
        Enum = {}; assert(not pcall(checkNativeConstants)); Enum = enums;
        Constants = false; assert(not pcall(checkNativeConstants)); Constants = constants;
        Constants.LFG_ROLEConstants.LFG_ROLE_NO_ROLE = 4;
        assert(not pcall(checkNativeConstants)); Constants.LFG_ROLEConstants.LFG_ROLE_NO_ROLE = -1;
        checkNativeConstants(); checkNativeConstants = nil;
    )lua");
    // Fresh VMs must reject pre-existing namespaces before publishing either
    // root, including when a conflicting table has the exact expected fields.
    for (const char* root : {"Enum", "Constants"}) {
        lua_State* fresh = luaL_newstate();
        require(fresh);
        lua_newtable(fresh); lua_setglobal(fresh, root);
        lua_pushcfunction(fresh, RovRetailUI::RegisterConstants);
        require(lua_pcall(fresh, 0, 0, 0) != 0); lua_pop(fresh, 1);
        lua_getglobal(fresh, !std::strcmp(root, "Enum") ? "Constants" : "Enum");
        require(lua_isnil(fresh, -1)); lua_pop(fresh, 1);
        lua_pushnil(fresh); lua_setglobal(fresh, root);
        lua_pushcfunction(fresh, RovRetailUI::RegisterConstants);
        require(lua_pcall(fresh, 0, 0, 0) == 0);
        lua_close(fresh);
    }
}


static bool bind(lua_State* L, int object, FrameScript_Object::ScriptIx& handler,
                 const char* method, const char* inherit = nullptr,
                 const char* function = nullptr, const char* body = nullptr) {
    XMLNode node(nullptr, "OnEvent");
    if (method) node.SetAttribute("method", method);
    if (inherit) node.SetAttribute("inherit", inherit);
    if (function) node.SetAttribute("function", function);
    if (body) { node.m_bodyLen = std::strlen(body); node.m_body = SStrDupA(body, __FILE__, __LINE__); }
    CStatus status;
    FrameScript_Object::ScriptData data{"return function(self,event,...) %s end"};
    const int top = lua_gettop(L);
    const bool ok = RovRetailUI::LoadScript(L, object, &node, handler, data, &status);
    require(lua_gettop(L) == top);
    require(ok == (status.m_highestSeverity < STATUS_ERROR));
    return ok;
}

static int invoke(lua_State* L, int object, FrameScript_Object::ScriptIx& handler) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, handler.m_luaRef);
    lua_rawgeti(L, LUA_REGISTRYINDEX, object);
    lua_pushstring(L, "EVENT");
    lua_pushnumber(L, 7);
    lua_pushnil(L);
    return lua_pcall(L, 4, 0, 0);
}

static void compositionFixture(lua_State* L) {
    run(L, R"lua(
        local base = { Method = function(self, ...) return self.value, select('#', ...), ... end };
        local inherited = setmetatable({ own = true }, { __index = base });
        local shared, key = {}, {};
        local first = { value = 1, shared = shared, [0] = 'ordinary-zero', [key] = false, [true] = 12 };
        local last = { value = 2, Method = base.Method };
        local target = setmetatable({}, { __index = base });
        assert(Mixin(target) == target);
        assert(select('#', Mixin(target, first, last)) == 1);
        assert(Mixin(target, target) == target);
        assert(target.value == 2 and target.shared == shared and target[key] == false and target[true] == 12);
        assert(target[0] == 'ordinary-zero' and first.value == 1);
        local copy = CreateFromMixins(first, inherited, last);
        assert(copy ~= target and copy ~= first and copy.shared == shared and copy.Method == base.Method);
        assert(copy.own and getmetatable(copy) == nil);
        assert(CreateFromMixins(inherited).Method == nil); -- fields, not prototype traversal
        local value, count, number, empty = copy:Method(7, nil);
        assert(value == 2 and count == 2 and number == 7 and empty == nil);
        assert(next(CreateFromMixins()) == nil and CreateFromMixins() ~= CreateFromMixins());
        for _, invalid in ipairs({false, 7, 'invalid', function() end}) do
            assert(not pcall(Mixin, invalid, {}));
            assert(not pcall(Mixin, {}, invalid));
            assert(not pcall(CreateFromMixins, invalid));
        end
        assert(not pcall(Mixin) and not pcall(Mixin, nil));
        assert(not pcall(CreateFromMixins, nil));
        local partial = {};
        assert(not pcall(Mixin, partial, {applied = true}, nil, {unreached = true}));
        assert(partial.applied and partial.unreached == nil);
        local redirected = {};
        local proxy = setmetatable({}, { __newindex = redirected });
        assert(Mixin(proxy, {field = shared}) == proxy and redirected.field == shared and rawget(proxy, 'field') == nil);
        local failure = {};
        local rejecting = setmetatable({}, { __newindex = function() error(failure) end });
        local ok, err = pcall(Mixin, rejecting, {field = true});
        assert(not ok and err == failure);
        -- Existing inherited method lookup still works when composition adds a
        -- different method; composition never replaces the object's metatable.
        local methodTarget = setmetatable({value = 9}, { __index = base });
        Mixin(methodTarget, {Other = function(self) return self:Method() end});
        assert(methodTarget:Other() == 9 and rawget(methodTarget, 'Method') == nil);
        local mix, create = Mixin, CreateFromMixins;
        assert(not pcall(hooksecurefunc, 'Mixin', function() end));
        assert(not pcall(hooksecurefunc, {Alias = create}, 'Alias', function() end));
        assert(Mixin == mix and CreateFromMixins == create);
        local old = {Call = function() return 3 end}; local called = false;
        hooksecurefunc(old, 'Call', function() called = true end);
        assert(old.Call() == 3 and called);
        -- Lua owns shallow references; the native API keeps no registry roots.
        local weak = setmetatable({}, {__mode = 'v'});
        do
            local source = {payload = {}, Call = function() return 42 end};
            local object = CreateFromMixins(source);
            weak[1], weak[2], weak[3] = source, object, object.payload;
            source = nil; collectgarbage('collect');
            assert(weak[1] == nil and weak[2] == object and weak[3] == object.payload and object:Call() == 42);
        end
        collectgarbage('collect');
        assert(weak[1] == nil and weak[2] == nil and weak[3] == nil);
        local match = {}; local entries = {[0] = false, [-7] = match, [12] = 'sparse', field = 19};
        assert(table.contains(entries, false) == true and table.contains(entries, match) == true);
        assert(table.contains(entries, 'sparse') == true and table.contains(entries, 19) == true);
        assert(table.contains(entries, {}) == false and table.contains(entries, '19') == false);
        assert(table.contains(entries, nil) == false and table.contains({}) == false);
        assert(table.contains(setmetatable({}, {__index = {field = 19}}), 19) == false);
        assert(select('#', table.contains(entries, 19)) == 1);
        assert(not pcall(table.contains) and not pcall(table.contains, 'invalid', 19));
        local meta = {__eq = function(left, right) return left.id == right.id end};
        assert(table.contains({setmetatable({id = 8}, meta)}, setmetatable({id = 8}, meta)));
        meta.__eq = function() error(failure) end;
        local equalOk, equalError = pcall(table.contains, {setmetatable({}, meta)}, setmetatable({}, meta));
        assert(not equalOk and equalError == failure);
    )lua");
    // Real FrameScript object registration/retirement, without claiming this
    // test object is a gameplay frame or constructing a replacement HUD.
    struct Object : FrameScript_Object {
        int meta;
        explicit Object(int ref) : meta(ref) {}
        const char* GetName() override { return "CompositionFixture"; }
        int32_t GetScriptMetaTable() override { return meta; }
        bool IsA(int32_t) override { return false; }
    };
    lua_newtable(L);
    const int meta = luaL_ref(L, LUA_REGISTRYINDEX);
    {
        Object object(meta);
        object.RegisterScriptObject("nativeObject");
        lua_rawgeti(L, LUA_REGISTRYINDEX, object.GetLuaObjectRef());
        lua_rawgeti(L, -1, 0);
        require(lua_touserdata(L, -1) == &object);
        lua_pop(L, 2);
        run(L, R"lua(
            local identity = nativeObject[0];
            assert(Mixin(nativeObject, {Method = function(self) return self[0] end}) == nativeObject);
            assert(nativeObject:Method() == identity);
            assert(not pcall(Mixin, nativeObject, {[0] = false}));
            assert(not pcall(CreateFromMixins, nativeObject));
            assert(nativeObject[0] == identity);
            retainedObject = nativeObject;
        )lua");
        object.UnregisterScriptObject("nativeObject");
        require(object.m_luaObjectRef == LUA_NOREF);
        run(L, "assert(nativeObject == nil and retainedObject[0] == nil); retainedObject = nil; collectgarbage('collect')");
    }
    luaL_unref(L, LUA_REGISTRYINDEX, meta);
    static const char owner[] = "CompositionOwner";
    lua_tainted = owner;
    run(L, "taintedMixin = {Method = function() return 'owned' end}; taintedCopy = CreateFromMixins(taintedMixin)");
    require(lua_tainted == owner);
    lua_tainted = nullptr;
    run(L, "local secure, owner = issecurevariable(taintedCopy, 'Method'); assert(not secure and owner == 'CompositionOwner'); assert(taintedCopy.Method == taintedMixin.Method)");
    lua_tainted = owner;
    run(L, "local ok = pcall(Mixin, {}, nil); assert(not ok)");
    require(lua_tainted == owner);
    lua_tainted = nullptr;
    run(L, "taintedMixin = nil; taintedCopy = nil; collectgarbage('collect')");
    unsetenv("WOWDECOMP_RETAIL_UI_COMPAT");
    run(L, "assert(not pcall(Mixin, {}, {}) and not pcall(CreateFromMixins, {}) and not pcall(table.contains, {}, 1))");
    setenv("WOWDECOMP_RETAIL_UI_COMPAT", RovRetailUI::Version, 1);
}

static void delegateFixture(lua_State* L) {
    run(L, R"lua(
        local echo = function(...) return ... end;
        local delegate = CreateSecureDelegate(echo);
        local object, callback = {}, function() return 19 end;
        local a, b, c, d, e = delegate(nil, object, callback, false, nil);
        assert(a == nil and b == object and c == callback and d == false and e == nil);
        assert(select('#', delegate()) == 0 and select('#', delegate(nil, nil)) == 2);
        assert(select('#', delegate(1, nil, 3, nil)) == 4);
        assert(CreateSecureDelegate(echo, nil)(7) == 7);
        assert(CreateSecureDelegate(echo, {})(8) == 8);
        assert(CreateSecureDelegate(echo, {wrapUntrustedFunctions = true})(9) == 9);
        local args = {}; for i = 1, 500 do args[i] = i end;
        assert(select('#', delegate(unpack(args))) == 500);
        assert(not pcall(CreateSecureDelegate) and not pcall(CreateSecureDelegate, nil));
        for _, invalid in ipairs({false, 3, 'function', {}, print}) do
            assert(not pcall(CreateSecureDelegate, invalid));
        end
        for _, options in ipairs({false, 3, 'options', echo,
            {wrapUntrustedFunctions = false}, {wrapUntrustedFunctions = 1},
            {unknown = true}, {secret = true}, {securityContext = 'secure'},
            {['wrapUntrustedFunctions\000extra'] = true}, {[1] = true},
            setmetatable({}, {__index = function() error('must not inspect') end})}) do
            assert(not pcall(CreateSecureDelegate, echo, options));
        end
        assert(not pcall(CreateSecureDelegate, echo, nil, true));
        local failure = {};
        local throwing = CreateSecureDelegate(function() error(failure) end);
        local ok, err = pcall(throwing); assert(not ok and err == failure);
        -- A failing user error handler cannot bypass delegate cleanup.
        assert(not xpcall(throwing, function() error('handler failed') end));
        assert(delegate(17) == 17);
        local depth, recursive = 0;
        recursive = CreateSecureDelegate(function(n)
            depth = depth + 1;
            if n == 0 then return nil, depth, nil end;
            return recursive(n - 1);
        end);
        local x, y, z = recursive(10); assert(x == nil and y == 11 and z == nil);
        local nested = CreateSecureDelegate(function()
            local ok, err = pcall(throwing); assert(not ok and err == failure);
            return delegate(nil, 21, nil);
        end);
        assert(select('#', nested()) == 3 and select(2, nested()) == 21);
        local closed;
        local unwind = CreateSecureDelegate(function()
            local value = 25; closed = function() return value end;
            error(failure);
        end);
        assert(not pcall(unwind)); collectgarbage('collect'); assert(closed() == 25);
        local factory = CreateSecureDelegate;
        assert(not pcall(hooksecurefunc, 'CreateSecureDelegate', function() end));
        assert(not pcall(hooksecurefunc, {Alias = factory}, 'Alias', function() end));
        assert(not pcall(hooksecurefunc, {Alias = delegate}, 'Alias', function() end));
        assert(CreateSecureDelegate == factory and delegate(26) == 26);
        local weak = setmetatable({}, {__mode = 'v'});
        do
            local payload = {};
            local function retained() return payload end;
            local owner = CreateSecureDelegate(retained);
            weak[1], weak[2], weak[3] = retained, owner, payload;
            retained = nil; collectgarbage('collect');
            assert(weak[1] ~= nil and weak[2] == owner and weak[3] == owner());
        end
        collectgarbage('collect');
        assert(weak[1] == nil and weak[2] == nil and weak[3] == nil);
        delegateCalls = 0;
        delegateCallee = function(...) delegateCalls = delegateCalls + 1; return ... end;
        retainedDelegate = CreateSecureDelegate(delegateCallee);
        function insecureDuringDelegate()
            forceinsecure(); assert(not issecure());
        end
        insecureDelegate = CreateSecureDelegate(insecureDuringDelegate);
        function insecureErrorDuringDelegate()
            forceinsecure(); error('insecure delegate failure');
        end
        insecureErrorDelegate = CreateSecureDelegate(insecureErrorDuringDelegate);
    )lua");
    static const char owner[] = "DelegateOwner";
    const int expected = lua_taintexpected;
    const char* gc = lua_gctag;
    run(L, "insecureDelegate()");
    require(!lua_tainted && lua_taintexpected == expected && !lua_taintedclosure);
    require(lua_gctag == gc && !lua_closuretaint);
    lua_taintexpected = 7;
    lua_gctag = owner;
    run(L, "insecureDelegate()");
    require(!lua_tainted && lua_taintexpected == 7 && lua_gctag == owner && !lua_taintedclosure);
    lua_taintexpected = expected;
    lua_gctag = gc;
    run(L, "assert(not pcall(insecureErrorDelegate)); assert(retainedDelegate(27) == 27)");
    require(!lua_tainted && lua_taintexpected == expected && !lua_taintedclosure);

    // Admission examines real VM value/closure taint, including a callee whose
    // environment was changed after its delegate had already been constructed.
    lua_getglobal(L, "delegateCallee");
    lua_pushvalue(L, LUA_GLOBALSINDEX);
    lua_tainted = owner;
    require(lua_setfenv(L, -2));
    lua_tainted = nullptr;
    lua_pop(L, 1);
    run(L, "assert(not pcall(retainedDelegate)); assert(not pcall(CreateSecureDelegate, delegateCallee))");
    run(L, "delegateCallee = function(...) delegateCalls = delegateCalls + 1; return ... end; retainedDelegate = CreateSecureDelegate(delegateCallee)");
    // Even with propagation disabled outside the boundary, option fields have
    // their own value taint and must not be silently accepted as trusted.
    lua_getglobal(L, "CreateSecureDelegate");
    lua_getglobal(L, "delegateCallee");
    lua_newtable(L);
    lua_pushboolean(L, 1);
    lua_settaint(L, -1, owner);
    lua_setfield(L, -2, "wrapUntrustedFunctions");
    require(lua_pcall(L, 2, 0, 0) != 0);
    lua_pop(L, 1);
    lua_getglobal(L, "retainedDelegate");
    lua_pushstring(L, "tainted-input");
    lua_settaint(L, -1, owner);
    require(lua_pcall(L, 1, 0, 0) != 0);
    lua_pop(L, 1);
    lua_getglobal(L, "retainedDelegate");
    lua_getglobal(L, "print");
    lua_settaint(L, -1, owner);
    require(lua_pcall(L, 1, 0, 0) != 0);
    lua_pop(L, 1);
    lua_tainted = owner;
    run(L, "local ok = pcall(CreateSecureDelegate, function() end); assert(not ok); assert(not pcall(retainedDelegate))");
    require(lua_tainted == owner && lua_taintexpected == expected);
    lua_tainted = nullptr;
    lua_closuretaint = owner;
    run(L, "assert(not pcall(CreateSecureDelegate, function() end)); taintedDelegateCreator = function() return CreateSecureDelegate(delegateCallee) end; taintedDelegateCaller = function() return retainedDelegate() end");
    require(lua_closuretaint == owner);
    lua_closuretaint = nullptr;
    run(L, "assert(not pcall(taintedDelegateCreator)); assert(not pcall(taintedDelegateCaller)); taintedDelegateCreator = nil; taintedDelegateCaller = nil");
    run(L, "assert(delegateCalls == 1); assert(retainedDelegate(28) == 28); assert(delegateCalls == 2)");
    run(L, "local f = CreateSecureDelegate(function() forceinsecure(); return {} end); delegateTaintedResult = f(); local secure, owner = issecurevariable('delegateTaintedResult'); assert(not secure and owner); delegateTaintedResult = nil");
    unsetenv("WOWDECOMP_RETAIL_UI_COMPAT");
    run(L, "assert(not pcall(CreateSecureDelegate, function() end)); assert(not pcall(retainedDelegate))");
    setenv("WOWDECOMP_RETAIL_UI_COMPAT", RovRetailUI::Version, 1);
    run(L, "retainedDelegate = nil; delegateCallee = nil; insecureDelegate = nil; insecureErrorDelegate = nil; insecureDuringDelegate = nil; insecureErrorDuringDelegate = nil; teardownDelegate = CreateSecureDelegate(function() return {} end); collectgarbage('collect')");
    require(lua_taintexpected == expected && !lua_tainted && !lua_taintedclosure);
}

static void fixture(lua_State* L) {
    run(L, R"lua(
        result = '';
        A = { value = 1, First = function(self, event, number, empty)
            assert(event == 'EVENT' and number == 7 and empty == nil);
            result = result .. 'A'; self.calls = (self.calls or 0) + 1;
        end };
        B = { value = 2, Second = function(self, event, ...)
            assert(select('#', ...) == 2); result = result .. 'B';
        end };
        Invalid = { [0] = 'overwrite' };
        Bad = { Fail = function() error('expected-handler-failure') end };
        GlobalHandler = function() result = result .. 'G' end;
        target = { [0] = 'native-identity', original = true };
    )lua");
    lua_getglobal(L, "target");
    const int object = luaL_ref(L, LUA_REGISTRYINDEX);
    auto mix = [&](const char* names) {
        CStatus status;
        const int top = lua_gettop(L);
        const bool ok = RovRetailUI::ApplyMixins(L, object, names, &status);
        require(lua_gettop(L) == top);
        require(ok == (status.m_highestSeverity < STATUS_ERROR));
        return ok;
    };
    unsetenv("WOWDECOMP_RETAIL_UI_COMPAT");
    require(!mix("A"));
    XMLNode ignored(nullptr, "OnEvent");
    ignored.SetAttribute("method", "First");
    ignored.SetAttribute("inherit", "append");
    require(!RovRetailUI::HasScriptAttributes(&ignored));
    setenv("WOWDECOMP_RETAIL_UI_COMPAT", "wrong-version", 1);
    require(!mix("A"));
    require(!RovRetailUI::HasScriptAttributes(&ignored));
    setenv("WOWDECOMP_RETAIL_UI_COMPAT", RovRetailUI::Version, 1);
    require(RovRetailUI::HasScriptAttributes(&ignored));
    XMLNode ordinary(nullptr, "OnLoad");
    require(!RovRetailUI::HasScriptAttributes(&ordinary));
    for (const char* names : {"A, Missing", "A, Invalid", "A,", "A,,B", "A.B", "", "A; error('injection')"}) {
        require(!mix(names));
        run(L, "assert(target.value == nil and target[0] == 'native-identity')");
    }
    require(mix(" A,\tB, Bad "));
    run(L, "assert(target.value == 2 and target.original and target[0] == 'native-identity')");
    {
        FrameScript_Object::ScriptIx handler;
        require(bind(L, object, handler, "First"));
        require(bind(L, object, handler, "Second", "append"));
        require(invoke(L, object, handler) == 0);
        run(L, "assert(result == 'AB' and target.calls == 1); result = ''");
        require(bind(L, object, handler, nullptr, "prepend", "GlobalHandler"));
        require(invoke(L, object, handler) == 0);
        run(L, "assert(result == 'GAB'); result = ''");
        require(bind(L, object, handler, nullptr, "append", nullptr, "assert(event == 'EVENT'); result = result .. 'I'"));
        require(invoke(L, object, handler) == 0);
        run(L, "assert(result == 'GABI'); result = ''");
        const int old = handler.m_luaRef;
        require(!bind(L, object, handler, "Missing"));
        require(!bind(L, object, handler, "First", "invalid"));
        require(!bind(L, object, handler, "First", nullptr, "GlobalHandler"));
        require(!bind(L, object, handler, "First", nullptr, nullptr, "result = ''"));
        require(handler.m_luaRef == old);
        unsetenv("WOWDECOMP_RETAIL_UI_COMPAT");
        require(!bind(L, object, handler, "First"));
        require(handler.m_luaRef == old);
        setenv("WOWDECOMP_RETAIL_UI_COMPAT", RovRetailUI::Version, 1);
        // First failure propagates through pcall and does not invoke the tail.
        require(bind(L, object, handler, "Fail", "prepend"));
        require(invoke(L, object, handler) != 0);
        require(std::strstr(lua_tostring(L, -1), "expected-handler-failure"));
        lua_pop(L, 1);
        run(L, "assert(result == '')");
        require(bind(L, object, handler, "First")); // ordinary override retires chain
        require(invoke(L, object, handler) == 0);
        run(L, "assert(result == 'A'); result = ''");
    }
    // Never erase or merge a different script owner to make inheritance pass.
    {
        FrameScript_Object::ScriptIx handler;
        static const char owner[] = "RetailFixtureOwner";
        lua_tainted = owner;
        require(mix("A"));
        require(bind(L, object, handler, "First"));
        require(handler.m_tainted == owner && lua_tainted == owner);
        lua_tainted = nullptr;
        require(!bind(L, object, handler, "Second", "append"));
        require(handler.m_tainted == owner);
    }
    luaL_unref(L, LUA_REGISTRYINDEX, object);
    lua_gc(L, LUA_GCCOLLECT, 0);
}

static bool loadReference(lua_State* L, const char* path) {
    std::ifstream input(path, std::ios::binary);
    require(input.good());
    std::string source((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    require(source.size() < 1024 * 1024);
    const std::string chunk = std::string("@") + path;
    if (luaL_loadbuffer(L, source.data(), source.size(), chunk.c_str()) || lua_pcall(L, 0, 0, 0)) {
        std::fprintf(stderr, "reference-load-failed: %s\n", lua_tostring(L, -1));
        lua_pop(L, 1);
        return false;
    }
    std::printf("reference-loaded: %s\n", path);
    return true;
}

static bool helpers(lua_State* L, const char* root) {
    // Independent helper qualification, in relative SharedXMLBase TOC order.
    // This intentionally does not claim full addon/bootstrap closure.
    for (const char* name : {"Mixin.lua", "TableUtil.lua", "EnumUtil.lua", "FunctionUtil.lua", "MathUtil.lua", "AnchorUtil.lua"}) {
        const std::string path = std::string(root) + "/Interface/AddOns/Blizzard_SharedXMLBase/" + name;
        if (!loadReference(L, path.c_str())) return false;
    }
    const char* checks = R"lua(
        local object = CreateAndInitFromMixin({Init = function(self, ...) self.count = select('#', ...); self.arg = ... end}, 17, nil);
        assert(object.count == 2 and object.arg == 17);
        local base = {Method = function(self) return self.value end, value = 5};
        local native = CreateFromMixins(base, {value = 8});
        local luaCopy = SecureMixin({}, base, {value = 8});
        assert(native.Method == luaCopy.Method and native.value == luaCopy.value and native:Method() == 8);
        local copy = CreateSecureMixinCopy(base);
        assert(copy.Method == base.Method and getmetatable(copy) == false);
        local enum = EnumUtil.MakeEnum('One', 'Two');
        assert(enum.One == 1 and enum.Two == 2 and EnumUtil.IsValid(enum, 2) and not EnumUtil.IsValid(enum, 3));
        local relative = {};
        local anchor = AnchorUtil.CreateAnchor('CENTER', relative, 'BOTTOM', 4, -9);
        local point, target, relativePoint, x, y = anchor:Get();
        assert(point == 'CENTER' and target == relative and relativePoint == 'BOTTOM' and x == 4 and y == -9);
        anchor:SetOffsets(11, 12);
        assert(select(4, anchor:Get()) == 11 and select(5, anchor:Get()) == 12);
        local default = CreateAnchor();
        assert(select(1, default:Get()) == 'TOPLEFT' and select(4, default:Get()) == 0);
        local grid = AnchorUtil.CreateGridLayout(nil, 3, 2, 4);
        assert(grid.stride == 3 and grid.paddingX == 2 and grid.paddingY == 4);
        assert(grid.direction == GridLayoutMixin.Direction.TopLeftToBottomRight);
        assert(select(1, grid:GetCustomOffset(2, 3)) == 0);
        grid:SetCustomOffsetFunction(function(row, col) return row * 2, col * 3 end);
        assert(select(1, grid:GetCustomOffset(2, 3)) == 4 and select(2, grid:GetCustomOffset(2, 3)) == 9);
        -- Helpers capture the native API once, exactly as the genuine source
        -- specifies. Later addon replacement must not alter those closures.
        local nativeMixin, nativeCreate = Mixin, CreateFromMixins;
        Mixin = function() error('replaced Mixin') end;
        CreateFromMixins = function() error('replaced CreateFromMixins') end;
        assert(CreateAndInitFromMixin({Init = function(self) self.ok = true end}).ok);
        assert(CreateSecureMixinCopy(base).Method == base.Method);
        Mixin, CreateFromMixins = nativeMixin, nativeCreate;
    )lua";
    if (luaL_loadstring(L, checks) || lua_pcall(L, 0, 0, 0)) {
        std::fprintf(stderr, "reference-helper-check-failed: %s\n", lua_tostring(L, -1));
        lua_pop(L, 1);
        return false;
    }
    std::puts("genuine-Forever-helpers=passed (Lua objects/anchor values; no frame, callback/pool or rendering claim)");
    return true;
}

// Inspect the genuine handler's lexical references without replacing any
// function or supplying missing debug/log/timer APIs. Keep this diagnostic in
// the probe; it is not a new addon API or a full error-handler execution claim.
static bool pushNamedUpvalue(lua_State* L, int function, const char* wanted) {
    for (int i = 1;; ++i) {
        const char* name = lua_getupvalue(L, function, i);
        if (!name) return false;
        if (!std::strcmp(name, wanted)) return true;
        lua_pop(L, 1);
    }
}

static bool scriptErrors(lua_State* L, const char* root) {
    const std::string path = std::string(root) + "/Interface/AddOns/Blizzard_ScriptErrors/Blizzard_ScriptErrors.lua";
    if (!loadReference(L, path.c_str())) return false;
    run(L, "assert(type(ScriptErrors) == 'table' and type(ScriptErrors.unhandledErrors) == 'table' and not ScriptErrors.isErrorHandlingDeferred)");
    lua_getglobal(L, "geterrorhandler");
    require(lua_pcall(L, 0, 1, 0) == 0);
    require(pushNamedUpvalue(L, 1, "GetErrorData"));
    require(pushNamedUpvalue(L, 2, "RemoveKStringsDelegate"));
    lua_setglobal(L, "referenceRemoveKStringsDelegate");
    lua_pop(L, 2);
    run(L, R"lua(
        assert(referenceRemoveKStringsDelegate('a|kb|Kc') == 'akbKc');
        assert(select('#', referenceRemoveKStringsDelegate('text')) == 1);
        local ok, err = pcall(referenceRemoveKStringsDelegate, {});
        assert(not ok and type(err) == 'string');
        assert(referenceRemoveKStringsDelegate('|Kafter error') == 'Kafter error');
        referenceRemoveKStringsDelegate = nil;
    )lua");
    std::puts("genuine-ScriptErrors=passed: construction/Init and actual captured RemoveKStringsDelegate success/error/recovery (trusted ordinary strings; no full error-handler/game claim)");
    return true;
}

static void reference(lua_State* L, const char* path) {
    require(loadReference(L, path));
    // These are table-only markers, not reconstructed gameplay frames. Verify
    // the genuine Camelot mixin's ordering through the production XML binder.
    run(L, "MainActionBar = {}; MicroMenu = {}; BagsBar = {}; OverrideActionBar = {}; MultiBarBottomLeft = {}; MicroMenuContainer = {}; target = {[0] = 'native-identity'}");
    lua_getglobal(L, "target");
    const int object = luaL_ref(L, LUA_REGISTRYINDEX);
    CStatus status;
    require(RovRetailUI::ApplyMixins(L, object, "EditModeUtil", &status));
    {
        FrameScript_Object::ScriptIx handler;
        require(bind(L, object, handler, "GetCenterManagedSystems"));
        lua_rawgeti(L, LUA_REGISTRYINDEX, handler.m_luaRef);
        lua_rawgeti(L, LUA_REGISTRYINDEX, object);
        require(lua_pcall(L, 1, 1, 0) == 0);
        lua_setglobal(L, "centers");
        run(L, "assert(#centers == 3 and centers[1] == MainActionBar and centers[2] == MicroMenu and centers[3] == BagsBar)");
        require(bind(L, object, handler, "GetBottomActionBarHierarchy"));
        lua_rawgeti(L, LUA_REGISTRYINDEX, handler.m_luaRef);
        lua_rawgeti(L, LUA_REGISTRYINDEX, object);
        require(lua_pcall(L, 1, 1, 0) == 0);
        lua_setglobal(L, "hierarchy");
        run(L, "assert(#hierarchy == 3 and hierarchy[1] == OverrideActionBar and hierarchy[2] == MultiBarBottomLeft and hierarchy[3] == MicroMenuContainer)");
    }
    luaL_unref(L, LUA_REGISTRYINDEX, object);
    std::puts("genuine-Camelot-EditModeUtil=passed (Lua table fixture; no gameplay/UI rendering claim)");
}

static void startResources() {
    require(SFile::Initialize());
    SFile::EnableDirectAccess(1); // Harness-only external files; no game archives mounted.
    CoordinateSetAspectRatio(4.0f / 3.0f);
    g_widthPixels = 1024; g_heightPixels = 768;
    // Match the production Gxu allocator/library ownership exactly: its
    // shutdown uses FT_Done_Library, not FT_Done_FreeType's allocator teardown.
    static FT_MemoryRec_ memory = {nullptr, FreeTypeAlloc, FreeTypeFree, FreeTypeRealloc};
    require(!g_FTLibrary && FT_New_Library(&memory, &g_FTLibrary) == 0);
    FT_Add_Default_Modules(g_FTLibrary);
    CSimpleFont::GetObjectType();
    CSimpleFont::CreateScriptMetaTable();
}

static void stopResources() {
    CSimpleFont::Destroy();
    CSimpleFont::DestroyScriptMetaTable();
    GxuFontShutdown();
    SFile::Destroy();
}

static bool resourceFile(const char* path) {
    CStatus status;
    const bool loaded = FrameXML_ProcessFileNoHash(path, nullptr, &status) && status.m_highestSeverity < STATUS_WARNING;
    char errors[4096];
    if (!loaded) std::fprintf(stderr, "native-resource-load-failed: %s: %s\n", path,
                              status.GetErrorStr(errors, sizeof(errors), STATUS_WARNING, "\n"));
    else std::printf("native-resource-loaded: %s\n", path);
    // Inspect the real registry after production traversal, including a partial
    // file failure. Reuse the engine XML tree solely to enumerate supplied
    // names; this does not construct resources or continue later scripts.
    std::ifstream input(path, std::ios::binary);
    std::string xml((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    XMLTree* tree = xml.size() < 1024 * 1024 ? XMLTree_Load(xml.data(), static_cast<uint32_t>(xml.size())) : nullptr;
    if (tree) {
        for (XMLNode* node = XMLTree_GetRoot(tree)->m_child; node; node = node->m_next) {
            if (std::strcmp(node->m_name.m_str, "FontFamily")) continue;
            const char* name = node->GetAttributeByName("name");
            CSimpleFont* font = name ? CSimpleFont::GetFont(name, 0) : nullptr;
            if (!font) continue;
            lua_State* L = FrameScript_GetContext();
            lua_rawgeti(L, LUA_REGISTRYINDEX, font->GetLuaObjectRef());
            lua_rawgeti(L, -1, 0);
            const bool canonical = lua_islightuserdata(L, -1) && lua_touserdata(L, -1) == font;
            lua_pop(L, 2);
            if (!canonical) {
                require(!loaded);
                std::printf("native-font-invalid-identity: %s\n", name);
                continue;
            }
            std::printf("native-font-observed: %s type=%s file=%s height=%g\n", name, font->GetObjectTypeName(),
                        font->m_attributes.m_font.m_str, font->m_attributes.m_fontHeight);
        }
        XMLTree_Free(tree);
    }
    return loaded;
}

static void resourceFixture(lua_State* L, const char* fontPath) {
    startResources();
    SFile::s_locale = 0x656e5553;
    char temp[] = "/tmp/rov-font-resources-XXXXXX";
    require(mkdtemp(temp));
    const std::filesystem::path directory = temp;
    const auto xml = directory / "fixture.xml";
    auto process = [&](const std::string& body, bool expected) {
        { std::ofstream out(xml); out << "<Ui>" << body << "</Ui>"; }
        require(resourceFile(xml.c_str()) == expected);
        require(lua_gettop(L) == 0);
    };
    const std::string member = std::string("<Member alphabet='roman'><Font font='") + fontPath + "' height='12'><Shadow x='1' y='-1'><Color r='0' g='0' b='0'/></Shadow></Font></Member>";
    const std::string family = "<FontFamily name='ResourceBase' virtual='true'>" + member + "</FontFamily>";
    process(family, true);
    CSimpleFont* base = CSimpleFont::GetFont("ResourceBase", 0);
    require(base && base->m_attributes.m_font.m_str);
    require(std::fabs(base->m_attributes.m_fontHeight - NDCToDDCWidth(12.0f / 1024)) < 0.000001f);
    run(L, "assert(ResourceBase:GetObjectType() == 'Font' and type(ResourceBase[0]) == 'userdata'); retainedFont = ResourceBase; assert(ResourceBase:GetFont())");
    process(family, true);
    require(CSimpleFont::GetFont("ResourceBase", 0) == base);
    run(L, "assert(ResourceBase == retainedFont)");
    std::string slugMember = member;
    slugMember.insert(slugMember.find(" height="), " slug='true'");
    process("<FontFamily name='ResourceSlug'>" + slugMember + "</FontFamily>", true);
    auto* slug = CSimpleFont::GetFont("ResourceSlug", 0);
    require(slug && (slug->m_attributes.m_fontFlags & RovSlug::TextFlag));
    HTEXTFONT handle = TextBlockGenerateFont(fontPath, slug->m_attributes.m_fontFlags, slug->m_attributes.m_fontHeight);
    require(handle);
    CGxFont* gx = TextBlockGetFontPtr(handle);
    require(gx && gx->UsesSlug() && !gx->UsesMsdf());
    unsigned glyphs = 0;
    for (unsigned code = 32; code < 127; ++code) {
        if (!FT_Get_Char_Index(FontFaceGetFace(gx->m_fontFaceHandle), code)) continue;
        GLYPHBITMAPDATA data;
        require(gx->GetGlyphData(&data, code));
        require(data.m_dataSize == RovSlug::Cell * RovSlug::Cell * sizeof(uint32_t));
        const auto* words = static_cast<const uint32_t*>(data.m_data);
        require(words[0] <= RovSlug::MaxCurves);
        if (code != 32) require(words[0] > 0);
        for (unsigned i = 1; i <= words[0] * 6; ++i) {
            float value; std::memcpy(&value, words + i, sizeof(value));
            require(std::isfinite(value) && value >= 0 && value <= 1);
        }
        require(data.m_glyphAdvance > 0 && data.m_glyphWidth > 0 && data.m_glyphHeight > 0);
        ++glyphs;
    }
    require(glyphs > 0);
    HandleClose(reinterpret_cast<HOBJECT>(handle));
    std::printf("native-slug-curves=passed: %u available ASCII glyphs; no GPU claim\n", glyphs);
    process("<FontFamily name='ResourceChild'><Member alphabet='roman'><Font inherits='ResourceBase'/></Member></FontFamily>", true);
    require(CSimpleFont::GetFont("ResourceChild", 0)->m_fontObject == base);
    std::string replacement = family;
    replacement.replace(replacement.find("height='12'"), std::strlen("height='12'"), "height='18'");
    process(replacement, true);
    require(std::fabs(base->m_attributes.m_fontHeight - NDCToDDCWidth(18.0f / 1024)) < 0.000001f);
    require(CSimpleFont::GetFont("ResourceChild", 0)->m_attributes.m_fontHeight == base->m_attributes.m_fontHeight);
    run(L, "assert(ResourceBase == retainedFont)");
    // Invalid selected resources cannot publish names or replace valid fonts.
    const auto before = base->m_attributes.m_fontHeight;
    for (const std::string& bad : {
        "<Member alphabet='roman'><Font font='missing-font.ttf' height='12'/></Member>",
        "<Member alphabet='roman'><Font inherits='MissingFont'/></Member>",
        "<Member alphabet='roman'><Font inherits='ResourceChild'/></Member>",
        "<Member alphabet='roman'><Font font='missing.ttf' height='nan'/></Member>",
        "<Member alphabet='roman'><Font inherits='ResourceChild' height='12'/></Member>",
        "<Member alphabet='roman'><Font inherits='ResourceChild' slug='true'/></Member>",
        "<Member alphabet='roman'><Font inherits='ResourceChild' slug='invalid'/></Member>",
        "<Member alphabet='roman'><Font inherits='ResourceChild'><Shadow x='1' y='-1'/></Font></Member>",
        "<Member alphabet='roman'><Font inherits='ResourceChild'><Unknown/></Font></Member>",
        "<Member alphabet='unknown'><Font inherits='ResourceChild'/></Member>"}) {
        process("<FontFamily name='ResourceBase'>" + bad + "</FontFamily>", false);
        require(base->m_attributes.m_fontHeight == before);
    }
    process("<FontFamily name='Rejected'>" + member + member + "</FontFamily>", false);
    require(!CSimpleFont::GetFont("Rejected", 0));
    process("<FontFamily name='Rejected'><Member alphabet='korean'><Font inherits='ResourceBase'/></Member></FontFamily>", false);
    SFile::s_locale = 0;
    process("<FontFamily name='Rejected'>" + member + "</FontFamily>", false);
    SFile::s_locale = 0x6b6f4b52;
    process("<FontFamily name='ResourceKorean'><Member alphabet='korean'><Font inherits='ResourceBase'/></Member></FontFamily>", true);
    SFile::s_locale = 0x656e5553;
    run(L, "ForeignResource = {};");
    process("<FontFamily name='ForeignResource'>" + member + "</FontFamily>", false);
    run(L, "retainedIdentity = ResourceBase[0]; ResourceBase[0] = nil");
    process(family, false);
    run(L, "ResourceBase[0] = retainedIdentity; retainedIdentity = nil");
    process("<FontFamily", false);
    // A rejected real resource must stop nested Include/Script traversal and
    // later TOC entries, not merely report an error after later Lua executes.
    const auto bad = directory / "bad.xml";
    const auto outer = directory / "outer.xml";
    const auto later = directory / "later.lua";
    const auto toc = directory / "fixture.toc";
    { std::ofstream out(bad); out << "<Ui><FontFamily name='Rejected'><Member alphabet='roman'><Font inherits='ResourceBase' slug='true'/></Member></FontFamily></Ui>"; }
    { std::ofstream out(later); out << "resourceFailureWasSkipped = true"; }
    { std::ofstream out(outer); out << "<Ui><Include file='" << bad.string() << "'/><Script file='" << later.string() << "'/></Ui>"; }
    { std::ofstream out(toc); out << outer.string() << '\n' << later.string() << '\n'; }
    CStatus tocStatus;
    MD5_CTX digest; MD5Init(&digest);
    require(!FrameXML_CreateFrames(toc.c_str(), nullptr, &digest, &tocStatus));
    require(tocStatus.m_highestSeverity >= STATUS_ERROR);
    run(L, "assert(resourceFailureWasSkipped == nil)");
    unsetenv("WOWDECOMP_RETAIL_UI_COMPAT");
    XMLNode disabled(nullptr, "FontFamily"); CStatus disabledStatus;
    require(!RovRetailUI::LoadFontFamily(&disabled, SFile::s_locale, &disabledStatus));
    setenv("WOWDECOMP_RETAIL_UI_COMPAT", RovRetailUI::Version, 1);
    // The same namespace and loader as the real application, with authored
    // inputs and ordinary direct filesystem access disabled.
    const auto bundle = directory / "bundle";
    const auto ui = bundle / "local-ui/Interface/AddOns/Fixture";
    std::filesystem::create_directories(ui);
    std::filesystem::create_directories(bundle / "assets/Fonts");
    std::filesystem::copy_file(fontPath, bundle / "assets/Fonts/Fixture.ttf");
    { std::ofstream out(ui / "font.xml"); out << "<Ui><FontFamily name='RuntimeFont'><Member alphabet='roman'><Font font='Fonts\\Fixture.ttf' height='18' slug='true'/></Member></FontFamily></Ui>"; }
    { std::ofstream out(bundle.parent_path() / "forever-runtime-plan.txt"); out << "# forever-70862 207a69936fcc6186deceaf63be07066c45b278339b2a4e266b1f1a1985cbeb74\nInterface/AddOns/Fixture/font.xml\n"; }
    setenv("WOWDECOMP_FOREVER_ASSETS", bundle.c_str(), 1);
    unsetenv("WOWDECOMP_FOREVER_STAGE");
    unsetenv("WOWDECOMP_FOREVER_PREVIEW");
    const bool diskCheck = System_SFile2::s_checkDisk;
    SFile::EnableDirectAccess(0);
    SFile::DisableSFileCheckDisk();
    require(!SFile::FileExistsEx("RovForeverFonts\\..\\local-ui\\Interface\\AddOns\\Fixture\\font.xml", 0));
    require(!SFile::FileExistsEx("RovForeverUI\\Interface\\AddOns\\Fixture\\missing.xml", 0));
    RovRetailUI::BootstrapRuntime(true);
    run(L, "assert(RuntimeFont and RuntimeFont:GetObjectType() == 'Font')");
    require(CSimpleFont::GetFont("RuntimeFont", 0)->m_attributes.m_fontFlags & RovSlug::TextFlag);
    unsetenv("WOWDECOMP_FOREVER_ASSETS");
    require(!SFile::FileExistsEx("RovForeverUI\\Interface\\AddOns\\Fixture\\font.xml", 0));
    System_SFile2::s_checkDisk = diskCheck;
    SFile::EnableDirectAccess(1);
    stopResources();
    run(L, "assert(ResourceBase == nil and ResourceChild == nil and ResourceKorean == nil and retainedFont[0] == nil); retainedFont = nil; ForeignResource = nil; collectgarbage('collect')");
    require(!CSimpleFont::GetFont("ResourceBase", 0));
    // A second complete resource-owner lifetime on the same live VM.
    startResources();
    process(family, true);
    stopResources();
    std::filesystem::remove_all(directory);
    std::puts("native-font-resources=passed: real CSimpleFont identities, FreeType files, inheritance, replacement, invalidation and repeated resource lifetimes (no GPU/gameplay claim)");
}

int main(int argc, char** argv) {
    const bool resourceSequence = argc >= 5 && !std::strcmp(argv[1], "--resource-sequence");
    require(resourceSequence || argc == 1 || (argc >= 3 && (!std::strcmp(argv[1], "--sequence") ||
        (argc == 3 && (!std::strcmp(argv[1], "--texture") || !std::strcmp(argv[1], "--resources") || !std::strcmp(argv[1], "--constants") || !std::strcmp(argv[1], "--reference") || !std::strcmp(argv[1], "--helpers") || !std::strcmp(argv[1], "--script-errors") || !std::strcmp(argv[1], "--load"))))));
    StormRtlInitialize();
    OsTimeManagerCreate(OsTimeManager::BestAvailable);
    for (int profile : {0, 1}) {
        for (const char* mode : {"", "wrong-version"}) {
            setenv("WOWDECOMP_RETAIL_UI_COMPAT", mode, 1);
            require(FrameScript_Initialize(profile));
            run(FrameScript_GetContext(), "assert(Enum == nil and Constants == nil and Mixin == nil and CreateFromMixins == nil and CreateSecureDelegate == nil and table.contains == nil); local t = {value = 1}; assert(t.value == 1); assert(securecall(function() return 31 end) == 31)");
            FrameScript_Destroy();
        }
        setenv("WOWDECOMP_RETAIL_UI_COMPAT", RovRetailUI::Version, 1);
        require(FrameScript_Initialize(profile));
        lua_State* L = FrameScript_GetContext();
        constantsFixture(L);
        compositionFixture(L);
        delegateFixture(L);
        fixture(L);
        bool loaded = true;
        if (argc >= 3) {
            if (resourceSequence) {
                startResources();
                std::filesystem::current_path(argv[2]); // Separate, supplied resource root.
                require(std::strlen(argv[3]) == 4);
                SFile::s_locale = 0;
                for (int i = 0; i < 4; ++i) SFile::s_locale = (SFile::s_locale << 8) | static_cast<unsigned char>(argv[3][i]);
                for (int i = 4; i < argc && loaded; ++i) {
                    const std::string path = argv[i];
                    loaded = path.size() >= 4 && path.substr(path.size() - 4) == ".lua" ? loadReference(L, argv[i]) : resourceFile(argv[i]);
                }
                stopResources();
            }
            else if (!std::strcmp(argv[1], "--resources")) resourceFixture(L, argv[2]);
            else if (!std::strcmp(argv[1], "--constants")) {
                run(L, "APIDocumentation = {AddDocumentationTable = function(self, value) self.value = value end}");
                const std::string path = std::string(argv[2]) + "/Interface/AddOns/Blizzard_APIDocumentationGenerated/LFGConstantsDocumentation.lua";
                loaded = loadReference(L, path.c_str());
                if (loaded) run(L, R"lua(
                    for _, t in ipairs(APIDocumentation.value.Tables) do
                        if t.Name == 'LFGRole' then
                            assert(t.NumValues == Enum.LFGRoleMeta.NumValues and t.MinValue == Enum.LFGRoleMeta.MinValue and t.MaxValue == Enum.LFGRoleMeta.MaxValue);
                            for _, field in ipairs(t.Fields) do assert(Enum.LFGRole[field.Name] == field.EnumValue) end
                        elseif t.Name == 'LFG_ROLEConstants' then
                            for _, field in ipairs(t.Values) do assert(Constants.LFG_ROLEConstants[field.Name] == field.Value) end
                        end
                    end
                    APIDocumentation = nil;
                )lua");
            }
            else if (!std::strcmp(argv[1], "--texture")) {
                const std::string path = std::string(argv[2]) + "/Interface/AddOns/Blizzard_SharedXMLBase/TextureUtil.lua";
                loaded = loadReference(L, path.c_str());
                if (loaded) run(L, R"lua(
                    assert(GetIconForRoleEnum(Enum.LFGRole.Tank) == GetIconForRole('TANK'));
                    assert(GetIconForRoleEnum(Enum.LFGRole.Healer) == GetIconForRole('HEALER'));
                    assert(GetIconForRoleEnum(Enum.LFGRole.Damage, true) == GetIconForRole('DAMAGER', true));
                    assert(GetIconForRoleEnum(Constants.LFG_ROLEConstants.LFG_ROLE_NO_ROLE) == GetIconForRole('GUIDE'));
                    assert(TextureUtil.GetSmallIconForRoleEnum(Enum.LFGRole.Tank) == TextureUtil.GetSmallIconForRole('TANK'));
                    local ok, err = pcall(GetIconForRole, 'invalid');
                    assert(not ok and string.find(err, 'Unknown role: invalid', 1, true));
                )lua");
            }
            else if (!std::strcmp(argv[1], "--load")) loaded = loadReference(L, argv[2]);
            else if (!std::strcmp(argv[1], "--helpers")) loaded = helpers(L, argv[2]);
            else if (!std::strcmp(argv[1], "--script-errors")) loaded = scriptErrors(L, argv[2]);
            else if (!std::strcmp(argv[1], "--reference")) reference(L, argv[2]);
            else {
                for (int i = 2; i < argc && loaded; ++i) {
                    const std::string path = argv[i];
                    if (path.size() < 4 || path.substr(path.size() - 4) != ".lua") {
                        std::fprintf(stderr, "ordered-load-unavailable: XML/resource construction requires an initialized client: %s\n", argv[i]);
                        loaded = false;
                    } else loaded = loadReference(L, argv[i]);
                }
            }
        }
        require(lua_gettop(L) == 0);
        FrameScript_Destroy();
        XMLTree_Cleanup();
        if (!loaded) { CStringManager::DestroyManager(); return 1; }
    }
    CStringManager::DestroyManager();
    std::puts("retail-compat-v1=passed: native composition, trusted-entry secure delegates, XML mixins/methods, errors, identity/taint/GC, disabled legacy VMs; both VM profiles");
}
