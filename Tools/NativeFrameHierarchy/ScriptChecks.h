#include "Engine/Source/lua-5.1/src/lua.h"
static void RegionLuaAt(const char* source, int line) {
    CStatus status;
    const auto name=std::string("RegionScriptFixture:")+std::to_string(line);
    int result = FrameScript_ExecuteBuffer(source, std::strlen(source), name.c_str(), &status, nullptr);
    if (!result || !status.IsEmpty()) {
        char error[2048];
        throw std::runtime_error(name+": "+status.GetErrorStr(error, sizeof(error), STATUS_WARNING, "; "));
    }
}
#define RegionLua(source) RegionLuaAt(source, __LINE__)
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
        assert(not pcall(f.CreateTexture,f,'RejectedTexture',nil,'MissingTextureTemplate'))
        assert(f:HasScript('OnKeyDown'))
        assert(t:GetText() == nil)
        local function text(input, expected)
            t:SetText(input)
            assert(t:GetText() == expected, input .. ': ' .. tostring(t:GetText()))
        end
        text('1 |4apple:apples;', '1 apple')
        text('2 |4apple:apples;', '2 apples')
        text('A|1은;는;', 'A는')
        text('B|1은;는;', 'B은')
        text('가|1은;는;', '가는')
        text('각|1은;는;', '각은')
        text('길|1으로;로;', '길로')
        text('|2Le roi', 'du roi')
        text('|2La reine', 'de la reine')
        text("|2L'ami", "de l'ami")
        text('|2Les enfants', 'des enfants')
        text('|2Étoile', "d'Étoile")
        text('||4literal;', '||4literal;')
        text('|cffffffff2|r |4item:items;', '|cffffffff2|r items')
        text('2 |4|2Le roi:|2Les rois;', '2 des rois')
        text('plain UTF-8: é 가 😀', 'plain UTF-8: é 가 😀')
        t:SetText(123); assert(t:GetText() == '123')
        t:SetText(1e100); assert(t:GetText() == tostring(1e100))
        t:SetFormattedText('%.2e',1e100); assert(t:GetText() == '1.00e+100')
        t:SetFormattedText('%u',4294967295); assert(t:GetText() == '4294967295')
        t:SetText(nil); assert(t:GetText() == nil)
        t:SetText(false); assert(t:GetText() == nil)
        t:SetFormattedText('%2$s %1$d |4item:items; %%', 2, 'Count')
        assert(t:GetText() == 'Count 2 items %')
        local formatted = t:GetText()
        t:SetText(string.rep('x',4095)); assert(#t:GetText() == 4095)
        t:SetText(formatted)
        local stable = t:GetText()
        for _,bad in ipairs({'|', '|1', '|1one;', '|2 ', "|2L'", '|2Le ', '|2La ', '|2Les ', '|4one:many', '|3-1(word)',
                            '2147483648 |4one:many;', string.rep('x',4096), string.rep('|2x ',1000),
                            string.char(0xc0,0x80), string.char(0xed,0xa0,0x80),
                            string.char(0xf0,0x9f), '|Tunsupported:10|t'}) do
            assert(not pcall(t.SetText,t,bad))
            assert(t:GetText() == stable)
        end
        assert(not pcall(t.SetFormattedText,t,'%q',1) and t:GetText() == stable)
        local nofont = f:CreateFontString()
        assert(not pcall(nofont.SetText,nofont,'without font'))
        t:SetText('|4 AV:AV Lua factory;')
        StaleFrame,StaleText,StaleTitle,StaleChild = f,t,title,NativeLuaFrameChild
    )");
    auto* text = static_cast<CSimpleFontString*>(CScriptObject::GetScriptObjectByName("NativeLuaFrameLabel", CSimpleFontString::GetObjectType()));
    Check(text && CSimpleFontString::s_allocator.m_dataUsed == 2, "Lua FontString bypassed original pool");
    // Authored content now flows through the original Lua setter and language
    // pipeline before original Gxu layout/cache/batch rendering.
    FrameScript_SetPluralRule(PLURAL_RULE_2);
    RegionLua("StaleText:SetText('21 |4one:few:many;'); assert(StaleText:GetText() == '21 one'); "
              "StaleText:SetText('22 |4one:few:many;'); assert(StaleText:GetText() == '22 few'); "
              "StaleText:SetText('11 |4one:few:many;'); assert(StaleText:GetText() == '11 many')");
    FrameScript_SetPluralRule(PLURAL_RULE_1);
    RegionLua("StaleText:SetText('0 |4one:many;'); assert(StaleText:GetText() == '0 one')");
    FrameScript_SetPluralRule(PLURAL_RULE_0);
    RegionLua("StaleText:SetText('|4 AV:AV Lua factory;')");
    auto pixels = Draw(device, root);
    Check(Ink(pixels) > 4, "Lua-created pooled FontString did not render");
    Equal(pixels, Draw(device, root));
    RegionLua(R"(
        SavedJustifyH, SavedJustifyV = StaleText:GetJustifyH(), StaleText:GetJustifyV()
        StaleText:SetJustifyH(SavedJustifyH == 'RIGHT' and 'LEFT' or 'RIGHT')
        StaleText:SetJustifyV(SavedJustifyV == 'TOP' and 'BOTTOM' or 'TOP')
        assert(StaleText:GetJustifyH() ~= SavedJustifyH and StaleText:GetJustifyV() ~= SavedJustifyV)
        local h,v = StaleText:GetJustifyH(), StaleText:GetJustifyV()
        assert(not pcall(StaleText.SetJustifyH,StaleText,'invalid'))
        assert(not pcall(StaleText.SetJustifyV,StaleText,{}))
        assert(StaleText:GetJustifyH() == h and StaleText:GetJustifyV() == v)
    )");
    auto justified = Draw(device, root);
    Check(Ink(justified) > 4 && justified.size() == pixels.size() && std::memcmp(justified.data(), pixels.data(), pixels.size() * sizeof(CImVector)), "Lua justification did not move actual glyph pixels");
    RegionLua("StaleText:SetJustifyH(SavedJustifyH); StaleText:SetJustifyV(SavedJustifyV)");
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
        RegionLua("assert(not pcall(NativeLuaFrame.CreateFontString,NativeLuaFrame,'RejectedTraversal')); assert(RejectedTraversal == nil); "
                  "assert(not pcall(StaleText.SetText,StaleText,'blocked'))");
    }
    RegionLua("assert(StaleText:GetText() == 'AV Lua factory')");
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
    std::cout << "frame-lua-pooled-fontstring-language-format-canonical-layout-rejection-render-teardown=passed\n";
}
