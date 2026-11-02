// Authored full agreement input sequences through the actual DXVK window/input
// owners. No original agreement API, Lua/XML replacement or game data is used.
#include "Engine/Source/Frame/Portable/WindowEvents.h"
#include "Engine/Source/Event/CMouseEvent.h"
#include "Engine/Source/Base/Status.h"
#include <memory>
#include <utility>
namespace AgreementInputChecks {
static void Check(bool good, const char* message) { if (!good) throw std::runtime_error(message); }
template<class F> static void Reject(F action) {
    bool failed = false;
    try { action(); } catch (const std::logic_error&) { failed = true; }
    Check(failed, "Unsafe agreement input operation admitted");
}
struct Guards {
    CSimpleTop& root;
    OsNative::Window& window;
    CSimpleSlider* slider;
    CSimpleTexture* header;
    static int Call(lua_State* L) {
        auto& self = *static_cast<Guards*>(lua_touserdata(L, lua_upvalueindex(1)));
        Reject([&] { Frame::Native::DispatchInputEvent(self.window, OSEVENT{OS_INPUT_MOUSE_UP,{MOUSE_BUTTON_LEFT,0,0,1}}); });
        Reject([&] { self.root.OnLayerUpdate(.016f); });
        Reject([&] { self.root.OnLayerRender(); });
        Reject([&] { Frame::Native::RequireDestroyable(); });
        Reject([&] { Frame::Native::RequireTreeMutation(); });
        Reject([&] { Frame::Native::ScrollRangeEdits edits; });
        CMouseEvent event{};
        const auto tracking = self.slider->m_buttonDown;
        Reject([&] { self.slider->OnLayerMouseDown(event, nullptr); });
        Check(tracking == self.slider->m_buttonDown, "Rejected nested Slider press changed tracking");
        { Frame::Native::ResizeScope resize;
          Reject([&] { Frame::Native::RequireRegionWidthMutation(self.header); }); }
        const auto animating=self.header->m_animating;
        self.header->m_animating=1;
        Reject([&] { Frame::Native::RequireRegionWidthMutation(self.header); });
        self.header->m_animating=animating;
        return 0;
    }
};
struct FailingSlider final : CSimpleSlider {
    bool fail = false;
    explicit FailingSlider(CSimpleFrame* parent) : CSimpleSlider(parent) {}
    int32_t OnLayerTrackUpdate(const CMouseEvent& event) override {
        if (fail) throw std::logic_error("authored native Slider tracking failure");
        return CSimpleSlider::OnLayerTrackUpdate(event);
    }
};
static void Run(CSimpleTop& root, OsNative::Window& window) {
    auto errors = FrameScript::Native::ScriptErrorCount();
    auto lua = [&](const char* source) {
        FrameScript_Execute(source, "authored complete agreement input", nullptr);
        if (FrameScript::Native::ScriptErrorCount() != errors)
            throw std::runtime_error("Agreement input fixture: " + FrameScript::Native::LastScriptError());
    };
    lua(R"(
        local p=CreateFrame('Frame','InputAgreement'); p:SetSize(180,180)
        p:SetPoint('BOTTOMLEFT',nil,'BOTTOMLEFT',10,10)
        local f=CreateFrame('ScrollFrame','InputScroll',p); f:SetSize(100,100)
        f:SetPoint('BOTTOMLEFT',p,'BOTTOMLEFT',10,60)
        local c=CreateFrame('Frame',nil,f); c:SetSize(100,300); f:SetScrollChild(c)
        local s=CreateFrame('Slider','InputSlider',f); s:SetSize(16,100)
        s:SetPoint('LEFT',f,'RIGHT',0,0); s:SetValueStep(1); s:SetMinMaxValues(0,200); s:SetValue(0)
        local t=s:CreateTexture(); t:SetSize(16,10); s:SetThumbTexture(t)
        local b=CreateFrame('Button','InputAgree',p); b:SetSize(60,20)
        b:SetPoint('BOTTOMLEFT',p,'BOTTOMLEFT',10,10); b:Disable()
        local header=p:CreateTexture('InputHeader'); header:SetSize(20,4); header:SetPoint('TOP')
        local title=p:CreateFontString('InputTitle'); title:SetWidth(73)
        local notice=p:CreateFontString('InputNotice')
        local foreignParent=CreateFrame('Frame',nil,p); foreignParent:Hide()
        local foreignHeader=foreignParent:CreateTexture()
        InputOrder={}; InputFail=false; InputClickFail=false; InputHide=false; InputNext=false
        f:SetScript('OnVerticalScroll',function(self,value)
            table.insert(InputOrder,'scroll'); s:SetValue(value)
            if value==200 then b:Enable() else b:Disable() end
        end)
        s:SetScript('OnValueChanged',function(self,value)
            table.insert(InputOrder,'value'); f:SetVerticalScroll(value)
            InputGuards()
            assert(not pcall(self.SetWidth,self,20))
            assert(not pcall(self.SetParent,self,nil))
            assert(not pcall(f.SetScrollChild,f,c))
            assert(not pcall(self.SetValueStep,self,0.5))
            assert(not pcall(header.SetWidth,header,90))
            assert(not pcall(CreateFrame,'Frame','RejectedAgreementFactory'))
            if InputHide then self:Hide() end
            if InputFail then error('authored protected Slider failure') end
        end)
        s:SetScript('OnMouseDown',function(self)
            table.insert(InputOrder,'down'); assert(not pcall(self.SetValue,self,10))
        end)
        s:SetScript('OnMouseUp',function(self)
            table.insert(InputOrder,'up'); assert(not pcall(self.SetValue,self,10))
        end)
        s:SetScript('OnLeave',function(self)
            assert(not pcall(self.SetValue,self,10)); assert(not pcall(header.SetWidth,header,90))
        end)
        f:EnableMouseWheel(true)
        f:SetScript('OnMouseWheel',function(self,delta)
            table.insert(InputOrder,'wheel'); s:SetValue(s:GetValue()-delta*50)
        end)
        b:SetScript('PreClick',function()
            table.insert(InputOrder,'pre'); assert(not pcall(header.SetWidth,header,90))
        end)
        b:SetScript('OnClick',function(self,key,down)
            table.insert(InputOrder,'click'); assert(key=='LeftButton')
            InputGuards(); assert(not pcall(p.SetWidth,p,90))
            p:Hide(); notice:Hide(); self:Disable()
            InputGuards(); assert(not pcall(foreignHeader.SetWidth,foreignHeader,73))
            assert(not pcall(header.SetWidth,{[0]=header[0]},73))
            assert(not pcall(header.SetWidth,header,0/0))
            assert(not pcall(header.SetHeight,header,20))
            assert(not pcall(header.SetPoint,header,'CENTER'))
            assert(not pcall(title.SetWidth,title,90))
            header:SetWidth(title:GetWidth()); table.insert(InputOrder,'width')
            if InputClickFail then error('authored protected Agree failure') end
            InputNext=true; p:Show(); table.insert(InputOrder,'tail')
        end)
        b:SetScript('PostClick',function() table.insert(InputOrder,'post') end)
        b:SetScript('OnLeave',function()
            table.insert(InputOrder,'leave'); assert(not pcall(InputHeader.SetWidth,InputHeader,90))
        end)
        p:SetScript('OnHide',function() table.insert(InputOrder,'hide') end)
        p:SetScript('OnShow',function()
            table.insert(InputOrder,'show'); assert(math.abs(InputHeader:GetWidth()-73)<0.01)
            assert(not pcall(InputHeader.SetWidth,InputHeader,90))
        end)
    )");
    auto frame = [&](const char* name) { return static_cast<CSimpleFrame*>(CScriptObject::GetScriptObjectByName(name, CSimpleFrame::GetObjectType())); };
    auto* panel = frame("InputAgreement");
    auto* scroll = static_cast<CSimpleScrollFrame*>(frame("InputScroll"));
    auto* slider = static_cast<CSimpleSlider*>(frame("InputSlider"));
    auto* button = static_cast<CSimpleButton*>(frame("InputAgree"));
    panel->SetFrameStrata(FRAME_STRATA_TOOLTIP);
    auto* header=static_cast<CSimpleTexture*>(CScriptObject::GetScriptObjectByName("InputHeader",CSimpleTexture::GetObjectType()));
    Guards guards{root,window,slider,header};
    auto* L = FrameScript_GetContext();
    lua_pushlightuserdata(L,&guards); lua_pushcclosure(L,Guards::Call,1); lua_setglobal(L,"InputGuards");
    int width,height,pw,ph; window.Size(width,height,pw,ph);
    auto deliver = [&](OSINPUT id,int x,int y,int key=0) {
        Frame::Native::DispatchInputEvent(window,OSEVENT{id,{key,x,y,2}});
        Check(!Frame::Native::IsMouseMotionInput() && !Frame::Native::IsMouseButtonInput(), "Agreement input retained a dispatch pin");
    };
    auto point = [&](CSimpleFrame* target, float fx=.5f, float fy=.5f) {
        CRect rect{}; Check(target->GetRect(&rect),"Agreement hit rectangle missing");
        float x,y; DDCToNDC(rect.minX+(rect.maxX-rect.minX)*fx,rect.minY+(rect.maxY-rect.minY)*fy,&x,&y);
        return std::pair<int,int>{int(x*width),int((1-y)*height)};
    };
    auto released = [&] {
        Check(!root.m_mouseCapture && !root.m_nativeButtonsDown && !slider->m_buttonDown &&
              !slider->m_isMouseDown && !slider->m_isMouseDragging && !button->m_inClick &&
              !root.m_mouseButtonName && !root.unused_1250, "Agreement input retained capture/tracking/click state");
    };
    auto cancel = [&] { deliver(OS_INPUT_CAPTURE_CHANGED,0,0); released(); };
    // Setup resets use the existing range-notification transaction; actual
    // press/motion/wheel/click delivery below supplies its own admission.
    auto reset = [&] {
        Frame::Native::ScrollRangeEdits edits;
        slider->SetValue(0);
        Check(FrameScript::Native::ScriptErrorCount()==errors,"Agreement setup reset failed");
    };
    root.OnLayerUpdate(.016f);
    auto [sx,sy]=point(slider);
    deliver(OS_INPUT_MOUSE_MOVE,sx,sy);
    Check(root.m_mouseFocus==slider,"Slider was not selected by actual input");
    deliver(OS_INPUT_MOUSE_DOWN,sx,sy,MOUSE_BUTTON_LEFT);
    lua("assert(table.concat(InputOrder,',')=='value,scroll,down'); InputOrder={}");
    Check(root.m_mouseCapture==slider && slider->m_buttonDown,"Slider press lost capture/tracking");
    deliver(OS_INPUT_MOUSE_MOVE,sx,sy+height);
    lua("assert(InputSlider:GetValue()==200 and InputScroll:GetVerticalScroll()==200 and InputAgree:IsEnabled()==1)");
    deliver(OS_INPUT_MOUSE_MOVE,sx,sy-height);
    lua("assert(InputSlider:GetValue()==0 and InputScroll:GetVerticalScroll()==0 and InputAgree:IsEnabled()==0)");
    deliver(OS_INPUT_MOUSE_MOVE,sx,sy-height); // No value notification at an unchanged clamped position.
    deliver(OS_INPUT_MOUSE_UP,sx,sy-height,MOUSE_BUTTON_LEFT);
    lua("assert(table.concat(InputOrder,',')=='value,scroll,value,scroll,up')"); released();
    // Cancellation and visibility retirement must also clear Slider tracking.
    deliver(OS_INPUT_MOUSE_MOVE,sx,sy); deliver(OS_INPUT_MOUSE_DOWN,sx,sy,MOUSE_BUTTON_LEFT); cancel();
    reset(); lua("InputHide=true");
    deliver(OS_INPUT_MOUSE_MOVE,sx,sy); deliver(OS_INPUT_MOUSE_DOWN,sx,sy,MOUSE_BUTTON_LEFT);
    Check(!root.m_mouseCapture && !slider->m_buttonDown,"Hidden Slider retained capture/tracking");
    deliver(OS_INPUT_MOUSE_UP,sx,sy,MOUSE_BUTTON_LEFT); released();
    lua("InputHide=false; InputSlider:Show()"); reset(); lua("InputFail=true");
    deliver(OS_INPUT_MOUSE_MOVE,sx,sy); deliver(OS_INPUT_MOUSE_DOWN,sx,sy,MOUSE_BUTTON_LEFT);
    Check(FrameScript::Native::ScriptErrorCount()==errors+1 && FrameScript::Native::LastScriptError().find("authored protected Slider failure")!=std::string::npos,"Protected Slider failure lost attribution");
    ++errors; cancel(); lua("InputFail=false"); reset();
    // Reject an impossible thumb span before publishing the Slider's down flag.
    auto* thumb=slider->m_thumbTexture; const auto oldHeight=thumb->m_height;
    thumb->SetHeight(slider->GetHeight()*2);
    root.OnLayerUpdate(.016f); deliver(OS_INPUT_MOUSE_MOVE,sx,sy);
    Reject([&] { deliver(OS_INPUT_MOUSE_DOWN,sx,sy,MOUSE_BUTTON_LEFT); });
    Check(!slider->m_buttonDown && slider->m_value==0,"Rejected Slider geometry changed value/tracking");
    cancel(); thumb->SetHeight(oldHeight); root.OnLayerUpdate(.016f);
    // Full wheel-to-bottom -> actual Button release -> next agreement transition.
    auto [wx,wy]=point(scroll); auto [bx,by]=point(button);
    auto wheelToBottom = [&] {
        reset(); lua("InputOrder={}");
        deliver(OS_INPUT_MOUSE_MOVE,wx,wy);
        for (int i=0;i<4;++i) deliver(OS_INPUT_MOUSE_WHEEL,0,0,-120);
        lua("assert(InputSlider:GetValue()==200 and InputAgree:IsEnabled()==1); InputOrder={}");
    };
    auto click = [&] {
        deliver(OS_INPUT_MOUSE_MOVE,bx,by);
        Check(root.m_mouseFocus==button,"Agree was not selected by actual input");
        deliver(OS_INPUT_MOUSE_DOWN,bx,by,MOUSE_BUTTON_LEFT);
        deliver(OS_INPUT_MOUSE_UP,bx,by,MOUSE_BUTTON_LEFT);
        released();
    };
    wheelToBottom(); click();
    lua("assert(InputNext and InputAgreement:IsShown()); assert(table.concat(InputOrder,',')=='pre,click,leave,hide,width,show,tail,post')");
    // A protected failure after width invalidation still retires capture and all
    // capabilities; restore the external UI state and repeat the entire sequence.
    lua("InputClickFail=true; InputNext=false"); wheelToBottom(); click();
    Check(FrameScript::Native::ScriptErrorCount()==errors+1 && FrameScript::Native::LastScriptError().find("authored protected Agree failure")!=std::string::npos,"Protected Agree failure lost attribution");
    ++errors;
    lua("InputClickFail=false; InputAgreement:Show()"); wheelToBottom(); click();
    lua("assert(InputNext and InputAgreement:IsShown())");
    lua(R"(
        local b=InputAgree; local handler=b:GetScript('OnClick')
        InputAgreement:Hide(); b:Enable()
        b:SetScript('OnClick',function() assert(not pcall(InputHeader.SetWidth,InputHeader,90)) end)
        b:Click(); b:SetScript('OnClick',handler); InputAgreement:Show()
    )");
    { Frame::Native::CallbackScope callback; Reject([&] { slider->SetValue(10); }); }
    { Frame::Native::ResizeScope resize; Reject([&] { deliver(OS_INPUT_MOUSE_MOVE,sx,sy); }); }
    { Frame::Native::CollectScope collect; Reject([&] { deliver(OS_INPUT_MOUSE_MOVE,sx,sy); }); }
    bool foreign=false; std::thread thread([&] { try { deliver(OS_INPUT_MOUSE_MOVE,sx,sy); } catch(const std::logic_error&) { foreign=true; } });
    thread.join(); Check(foreign,"Foreign Slider motion admitted");
    lua("InputAgree:SetScript('OnLeave',nil); InputSlider:SetScript('OnLeave',nil)"); cancel();
    // Native virtual exceptions during press and captured motion unwind the pins;
    // original cancellation remains available before a complete retry.
    {
        auto failing=std::make_unique<FailingSlider>(panel);
        failing->SetAllPoints(slider,1); failing->SetFrameLevel(slider->m_level+1,0);
        root.OnLayerUpdate(.016f); deliver(OS_INPUT_MOUSE_MOVE,sx,sy);
        Check(root.m_mouseFocus==failing.get(),"Native failure Slider not selected");
        failing->fail=true;
        Reject([&] { deliver(OS_INPUT_MOUSE_DOWN,sx,sy,MOUSE_BUTTON_LEFT); });
        Check(!failing->m_buttonDown && !Frame::Native::IsMouseButtonInput(),"Native press failure retained tracking/pin"); cancel();
        failing->fail=false; deliver(OS_INPUT_MOUSE_MOVE,sx,sy); deliver(OS_INPUT_MOUSE_DOWN,sx,sy,MOUSE_BUTTON_LEFT);
        failing->fail=true; Reject([&] { deliver(OS_INPUT_MOUSE_MOVE,sx+1,sy); });
        Check(!Frame::Native::IsMouseMotionInput(),"Native motion failure retained pin"); cancel();
        failing->fail=false; deliver(OS_INPUT_MOUSE_MOVE,sx,sy); deliver(OS_INPUT_MOUSE_DOWN,sx,sy,MOUSE_BUTTON_LEFT);
        deliver(OS_INPUT_MOUSE_MOVE,sx+1,sy); deliver(OS_INPUT_MOUSE_UP,sx+1,sy,MOUSE_BUTTON_LEFT); cancel();
    }
    delete panel;
    lua("InputGuards,InputOrder,InputFail,InputClickFail,InputHide,InputNext=nil,nil,nil,nil,nil,nil; collectgarbage('collect')");
    std::cout << "native-agreement-slider-press-motion-clamp-release-cancel-guards-recovery=passed\n";
    std::cout << "native-agreement-wheel-agree-next-header-callback-order-capture-recovery=passed\n";
}
}
