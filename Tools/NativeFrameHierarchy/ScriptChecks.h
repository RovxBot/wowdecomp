#include "Engine/Source/lua-5.1/src/lua.h"
static void RegionLua(const char* source) {
    CStatus status;
    int result = FrameScript_ExecuteBuffer(source, std::strlen(source), "RegionScriptFixture", &status, nullptr);
    if (!result || !status.IsEmpty()) {
        char error[2048];
        throw std::runtime_error(status.GetErrorStr(error, sizeof(error), STATUS_WARNING, "; "));
    }
}
static void ScriptChecks(LegacyDevice& device, CSimpleTop& root, CSimpleFont& font) {
    auto* parent = Create_SimpleFrame(nullptr);
    parent->SetName("NativeLuaFrame");
    auto* child = Create_SimpleFrame(parent);
    child->SetName("$parentChild");
    lua_State* L = FrameScript_GetContext();
    const int top = lua_gettop(L), expectedTaint = lua_taintexpected;
    lua_rawgeti(L, LUA_REGISTRYINDEX, font.GetLuaObjectRef());
    lua_setglobal(L, "RegionFixtureFont");
    RegionLua(R"(
        local f = NativeLuaFrame
        assert(f:GetObjectType() == 'Frame' and f:IsObjectType('Region'))
        assert(NativeLuaFrameChild:GetParent() == f and f:GetChildren() == NativeLuaFrameChild)
        f:SetSize(300, 150)
        f:SetPoint('BOTTOMLEFT', nil, 'BOTTOMLEFT', 10, 10)
        local w,h = f:GetSize()
        assert(math.abs(w-300) < .001 and math.abs(h-150) < .001)
        local before = f:GetNumRegions()
        assert(not pcall(f.CreateFontString, f, 'RejectedTemplate', nil, 'MissingVirtualTemplate'))
        assert(f:GetNumRegions() == before and RejectedTemplate == nil)
        assert(not pcall(f.CreateFontString, f, string.rep('x',1024)))
        local t = f:CreateFontString('$parentLabel', 'OVERLAY', RegionFixtureFont:GetName())
        assert(t == NativeLuaFrameLabel and t:GetName() == 'NativeLuaFrameLabel')
        assert(t:GetParent() == f and t:IsObjectType('FontString') and t:GetFontObject() == RegionFixtureFont)
        t:SetAllPoints('$parent')
        assert(f:GetNumRegions() == before+1 and f:GetRegions() == t)
        local point,relative = t:GetPoint()
        assert(point == 'TOPLEFT' and relative == f)
        assert(not pcall(t.GetPoint,t,0) and not pcall(t.GetPoint,t,1e100))
        assert(not pcall(t.SetPoint,t,'LEFT',t,'LEFT',0,0))
        assert(not pcall(t.SetPoint,t,'LEFT',{[0]=f[0]},'LEFT',0,0))
        assert(not pcall(t.SetAllPoints,t,{[0]=f[0]}))
        assert(not pcall(t.SetFontObject,t,{[0]=RegionFixtureFont[0]}))
        assert(not pcall(t.GetText,{[0]=t[0]}))
        assert(not pcall(t.SetParent,t,t) and not pcall(f.SetParent,f,NativeLuaFrameChild))
        assert(t:GetParent() == f and f:GetParent() == nil)
        local oldw,oldh = f:GetSize()
        assert(not pcall(f.SetSize,f,0/0,1) and not pcall(f.SetSize,f,1,math.huge))
        assert(not pcall(f.SetSize,f,100,-1))
        local neww,newh = f:GetSize()
        assert(neww == oldw and newh == oldh)
        local title = f:CreateTitleRegion()
        assert(title == f:GetTitleRegion() and title:GetParent() == f)
        title:SetAllPoints(f)
        assert(title == f:CreateTitleRegion())
        assert(not pcall(f.CreateTexture,f) and not pcall(f.SetScript,f,'OnKeyDown',function() end))
        assert(not pcall(t.SetText,t,'language caller remains closed') and t:GetText() == nil)
        StaleFrame,StaleText,StaleTitle,StaleChild = f,t,title,NativeLuaFrameChild
    )");
    auto* text = static_cast<CSimpleFontString*>(CScriptObject::GetScriptObjectByName("NativeLuaFrameLabel", CSimpleFontString::GetObjectType()));
    Check(text && CSimpleFontString::s_allocator.m_dataUsed == 1, "Lua FontString bypassed original pool");
    // LanguageProcess remains a separate prerequisite. Rendering uses explicit
    // authored C++ text with process=0, then real Lua visibility/anchor consumers.
    text->SetText("AV Lua factory", 0);
    auto pixels = Draw(device, root);
    Check(Ink(pixels) > 4, "Lua-created pooled FontString did not render");
    Equal(pixels, Draw(device, root));
    RegionLua("assert(StaleText:GetText() == 'AV Lua factory'); StaleText:Hide(); assert(not StaleText:IsShown())");
    Check(!Ink(Draw(device, root)), "Lua Hide retained text pixels");
    RegionLua("StaleText:Show(); assert(StaleText:IsVisible()); StaleText:SetFontObject(RegionFixtureFont)");
    Equal(pixels, Draw(device, root));
    RegionLua("NativeLuaFrame:Hide(); assert(not StaleText:IsVisible()); NativeLuaFrame:Show()");
    Equal(pixels, Draw(device, root));
    // Traversal admission must fail before allocation, stack/name/taint mutation.
    {
        Frame::Native::CollectScope traversal;
        RegionLua("assert(not pcall(NativeLuaFrame.CreateFontString,NativeLuaFrame,'RejectedTraversal')); assert(RejectedTraversal == nil)");
    }
    Check(lua_gettop(L) == top && lua_taintexpected == expectedTaint, "Region Lua left stack/taint nesting");
    delete parent;
    RegionLua(R"(
        assert(NativeLuaFrame == nil and NativeLuaFrameChild == nil and NativeLuaFrameLabel == nil)
        assert(not pcall(StaleFrame.GetName,StaleFrame) and not pcall(StaleText.GetText,StaleText))
        assert(not pcall(StaleTitle.GetSize,StaleTitle) and not pcall(StaleChild.GetParent,StaleChild))
        StaleFrame,StaleText,StaleTitle,StaleChild,RegionFixtureFont = nil,nil,nil,nil,nil
        collectgarbage('collect')
    )");
    Check(!CSimpleFrame::s_allocator.m_dataUsed && !CSimpleFontString::s_allocator.m_dataUsed,
          "Lua-created hierarchy retained pool slots");
    Check(!Ink(Draw(device, root)), "Destroyed Lua region retained batches");
    std::cout << "frame-lua-pooled-fontstring-canonical-layout-rejection-render-teardown=passed\n";
}
