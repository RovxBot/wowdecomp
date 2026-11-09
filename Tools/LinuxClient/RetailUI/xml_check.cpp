// Authored fixtures use the actual full-client Lua, XML nodes and compatibility
// implementation. Optional local reference execution never packages input files.
#include "Engine/Source/Frame/RetailUI/XMLCompat.h"
#include "Engine/Source/Base/Status.h"
#include "Engine/Source/Base/RCString.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/FrameXML/XMLTree.h"
#include "Engine/Source/Os/TimeManager.h"
#include "Engine/Source/Storm/H/storm.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

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
    run(L, "assert(not pcall(Mixin, {}, {}) and not pcall(CreateFromMixins, {}))");
    setenv("WOWDECOMP_RETAIL_UI_COMPAT", RovRetailUI::Version, 1);
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
    run(L, R"lua(
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
        local count = CreateCounter(10, 2); assert(count() == 12 and count() == 14);
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
    )lua");
    std::puts("genuine-Forever-helpers=passed (Lua objects/anchor values; no frame, callback/pool or rendering claim)");
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

int main(int argc, char** argv) {
    require(argc == 1 || (argc >= 3 && (!std::strcmp(argv[1], "--sequence") ||
        (argc == 3 && (!std::strcmp(argv[1], "--reference") || !std::strcmp(argv[1], "--helpers") || !std::strcmp(argv[1], "--load"))))));
    StormRtlInitialize();
    OsTimeManagerCreate(OsTimeManager::BestAvailable);
    for (int profile : {0, 1}) {
        for (const char* mode : {"", "wrong-version"}) {
            setenv("WOWDECOMP_RETAIL_UI_COMPAT", mode, 1);
            require(FrameScript_Initialize(profile));
            run(FrameScript_GetContext(), "assert(Mixin == nil and CreateFromMixins == nil); local t = {value = 1}; assert(t.value == 1)");
            FrameScript_Destroy();
        }
        setenv("WOWDECOMP_RETAIL_UI_COMPAT", RovRetailUI::Version, 1);
        require(FrameScript_Initialize(profile));
        lua_State* L = FrameScript_GetContext();
        compositionFixture(L);
        fixture(L);
        bool loaded = true;
        if (argc >= 3) {
            if (!std::strcmp(argv[1], "--load")) loaded = loadReference(L, argv[2]);
            else if (!std::strcmp(argv[1], "--helpers")) loaded = helpers(L, argv[2]);
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
    std::puts("retail-compat-v1=passed: native Lua composition, XML mixins/methods, errors, identity/taint/GC, disabled legacy VMs; both VM profiles");
}
