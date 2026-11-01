// Authored regression of the original Options Button -> top-level Show path.
// Original-data/operator execution is a separate acceptance requirement (#156).
#include "Engine/Source/Frame/Portable/WindowEvents.h"
#include "Engine/Source/Os/Portable/Window.h"
namespace OptionsChecks {
static void Check(bool good, const char* message) { if (!good) throw std::runtime_error(message); }
template<class F> static void Reject(F action) {
    bool failed = false;
    try { action(); } catch (const std::logic_error&) { failed = true; }
    Check(failed, "Unsafe Options callback operation admitted");
}
struct Guards {
    CSimpleTop& root;
    OsNative::Window& window;
    CSimpleFrame* panel;
    CSimpleButton* opener;
    static int Call(lua_State* L) {
        auto& self = *static_cast<Guards*>(lua_touserdata(L, lua_upvalueindex(1)));
        Check(Frame::Native::IsMouseButtonInput() && self.root.m_mouseCapture == self.opener,
              "Options Show lost the selected input/capture identity");
        Reject([&] { Frame::Native::RequireDestroyable(); });
        Reject([&] { Frame::Native::RequireTreeMutation(); });
        Reject([&] { self.root.CompressStrata(self.panel->m_strata); });
        Reject([&] { self.panel->SetFrameLevel(0, 1); });
        Reject([&] { self.panel->Raise(); });
        Reject([&] { self.root.OnLayerUpdate(.016f); });
        Reject([&] { self.root.OnLayerRender(); });
        Reject([&] { Frame::Native::RequireNoObjects(); });
        Reject([&] { Frame::Native::DispatchInputEvent(self.window, OSEVENT{OS_INPUT_MOUSE_UP,{MOUSE_BUTTON_LEFT,0,0,1}}); });
        self.root.m_moveResizeFrame = self.opener;
        Reject([&] { self.panel->Show(); });
        self.root.m_moveResizeFrame = nullptr;
        { Frame::Native::ResizeScope resize; Reject([&] { self.panel->Show(); }); }
        return 0;
    }
};
static void Run(CSimpleTop& root, OsNative::Window& window) {
    auto errors = FrameScript::Native::ScriptErrorCount();
    auto lua = [&](const char* source) {
        FrameScript_Execute(source, "authored Options input", nullptr);
        if (FrameScript::Native::ScriptErrorCount() != errors)
            throw std::runtime_error("Options regression: " + FrameScript::Native::LastScriptError());
    };
    lua(R"(
        local p=CreateFrame('Frame','OptionsPanel'); p:SetSize(100,100); p:SetPoint('CENTER');
        p:SetFrameLevel(0); p:Hide();
        local child=CreateFrame('Frame','OptionsChild',p); child:SetAllPoints(p);
        local other=CreateFrame('Frame','OptionsOccluder'); other:SetAllPoints(p);
        other:SetFrameLevel(3);
        local open=CreateFrame('Button','OptionsOpen'); open:SetSize(40,30);
        open:SetPoint('BOTTOMLEFT',nil,'BOTTOMLEFT',10,10);
        local close=CreateFrame('Button','OptionsClose',p); close:SetSize(40,30); close:SetPoint('CENTER');
        OptionsOrder={}; OptionsReject=false;
        open:SetScript('PreClick',function() table.insert(OptionsOrder,'pre') end);
        open:SetScript('OnClick',function(self)
            table.insert(OptionsOrder,'click');
            assert(not pcall(p.Raise,p));
            assert(not pcall(p.SetFrameLevel,p,0));
            assert(not pcall(p.SetParent,p,nil));
            assert(not pcall(p.SetWidth,p,20));
            assert(not pcall(CreateFrame,'Frame','ForbiddenOptionsFactory'));
            if OptionsReject then
                assert(not pcall(p.Show,p)); assert(not p:IsShown() and not p:IsVisible());
            else
                p:Show(); local level=p:GetFrameLevel(); p:Show();
                assert(p:GetFrameLevel()==level and p:IsVisible());
            end
            table.insert(OptionsOrder,'tail');
        end);
        open:SetScript('PostClick',function() table.insert(OptionsOrder,'post') end);
        child:SetScript('OnShow',function() table.insert(OptionsOrder,'child-show') end);
        p:SetScript('OnShow',function(self)
            assert(self:GetFrameLevel()>other:GetFrameLevel());
            assert(child:GetFrameLevel()==self:GetFrameLevel()+1);
            local level=self:GetFrameLevel(); self:Raise(); assert(level==self:GetFrameLevel());
            OptionsGuards(); table.insert(OptionsOrder,'panel-show');
        end);
        close:SetScript('OnClick',function() p:Hide() end);
        p:SetScript('OnHide',function() assert(not pcall(p.Raise,p)) end);
    )");
    auto frame = [&](const char* name) {
        return static_cast<CSimpleFrame*>(CScriptObject::GetScriptObjectByName(name, CSimpleFrame::GetObjectType()));
    };
    auto* panel = frame("OptionsPanel");
    auto* child = frame("OptionsChild");
    auto* occluder = frame("OptionsOccluder");
    auto* opener = static_cast<CSimpleButton*>(frame("OptionsOpen"));
    auto* close = static_cast<CSimpleButton*>(frame("OptionsClose"));
    // Original XML sets strata through C++; the Lua setter is not selected.
    panel->SetFrameStrata(FRAME_STRATA_DIALOG);
    occluder->SetFrameStrata(FRAME_STRATA_DIALOG);
    opener->SetFrameStrata(FRAME_STRATA_DIALOG);
    panel->SetFrameFlag(1, 1);
    Guards guards{root, window, panel, opener};
    auto* L = FrameScript_GetContext();
    lua_pushlightuserdata(L, &guards); lua_pushcclosure(L, Guards::Call, 1); lua_setglobal(L, "OptionsGuards");
    auto click = [&](CSimpleButton* button) {
        root.OnLayerUpdate(.016f);
        CRect rect{}; Check(button->GetRect(&rect), "Options input rectangle missing");
        float x, y; DDCToNDC((rect.minX+rect.maxX)/2, (rect.minY+rect.maxY)/2, &x, &y);
        int width, height, pixelWidth, pixelHeight; window.Size(width,height,pixelWidth,pixelHeight);
        auto deliver = [&](OSINPUT id, int key = 0) {
            Frame::Native::DispatchInputEvent(window, OSEVENT{id,{key,int(x*width),int((1-y)*height),2}});
        };
        deliver(OS_INPUT_MOUSE_MOVE);
        Check(root.m_mouseFocus == button, "Options input priority/focus mismatch");
        deliver(OS_INPUT_MOUSE_DOWN, MOUSE_BUTTON_LEFT);
        deliver(OS_INPUT_MOUSE_UP, MOUSE_BUTTON_LEFT);
        Check(!Frame::Native::IsMouseButtonInput() && !root.m_mouseCapture &&
              !root.m_nativeButtonsDown && !button->m_inClick && !button->m_isMouseDown,
              "Options workflow retained capture/callback state");
        Check(FrameScript::Native::ScriptErrorCount() == errors, "Options input callback failed");
    };
    for (bool down : {false, true}) {
        if (down) lua("OptionsOpen:RegisterForClicks('LeftButtonDown')");
        lua("OptionsOrder={}");
        click(opener);
        lua("assert(table.concat(OptionsOrder,',')=='pre,click,child-show,panel-show,tail,post')");
        click(close);
        Check(!panel->m_isShown && !panel->m_isVisible && !close->m_isVisible,
              "Options close failed to retire the subtree");
    }
    // Cross-stratum descendants must fit before the first visibility/list edit.
    child->SetFrameStrata(FRAME_STRATA_TOOLTIP); child->SetFrameLevel(128, 1);
    occluder->SetFrameLevel(40, 0);
    const auto oldPanelLevel = panel->m_level, oldOccluderLevel = occluder->m_level;
    const auto oldTop = root.m_strata[panel->m_strata]->m_topLevel;
    lua("OptionsReject=true"); click(opener);
    Check(panel->m_level == oldPanelLevel && occluder->m_level == oldOccluderLevel &&
          child->m_level == 128 && root.m_strata[panel->m_strata]->m_topLevel == oldTop &&
          !panel->m_isShown && !panel->m_isVisible && !child->m_isVisible,
          "Rejected Options raise changed visibility, compression or descendants");
    child->SetFrameStrata(panel->m_strata); child->SetFrameLevel(panel->m_level+1, 1);
    // Nested top-level subtrees and active movement retain their old rejection.
    child->SetFrameFlag(1, 1); click(opener); child->SetFrameFlag(1, 0);
    // Protected OnShow errors unwind all internal capabilities before recovery.
    lua("OptionsReject=false; OptionsPanel:SetScript('OnShow',function() error('Options authored failure') end)");
    ++errors; click(opener);
    Check(FrameScript::Native::LastScriptError().find("Options authored failure") != std::string::npos,
          "Options protected failure lost attribution");
    lua("OptionsPanel:Hide(); OptionsPanel:SetScript('OnShow',nil)");
    { Frame::Native::CallbackScope callback; Reject([&] { panel->Show(); }); }
    { Frame::Native::CollectScope collect; Reject([&] { panel->Show(); }); }
    { Frame::Native::ResizeScope resize; Reject([&] { panel->Show(); }); }
    bool foreign = false;
    std::thread thread([&] { try { panel->Show(); } catch (const std::logic_error&) { foreign = true; } });
    thread.join(); Check(foreign, "Foreign Options visibility admitted");
    lua("OptionsPanel:SetScript('OnHide',nil)");
    root.CancelMouseInput(); delete panel; delete opener; delete occluder;
    lua("OptionsGuards,OptionsOrder,OptionsReject=nil,nil,nil; collectgarbage('collect')");
    std::cout << "native-options-input-open-close-raise-order-preflight-rollback-guards=passed\n";
}
}
