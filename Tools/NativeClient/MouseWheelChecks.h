// Authored original-control chain through actual host input dispatch. No game data.
#include "Engine/Source/Frame/Portable/WindowEvents.h"
namespace MouseWheelChecks {
static void Check(bool good, const char* message) { if (!good) throw std::runtime_error(message); }
template<class F> static void Reject(F action) {
    bool failed = false;
    try { action(); } catch (const std::logic_error&) { failed = true; }
    Check(failed, "Unsafe wheel operation admitted");
}
struct Probe final : CSimpleFrame {
    CSimpleSlider* slider;
    bool fail = false;
    unsigned calls = 0;
    explicit Probe(CSimpleSlider* control) : CSimpleFrame(nullptr), slider(control) {}
    int32_t OnLayerMouseWheel(const CMouseEvent&) override {
        ++calls;
        // The virtual consumer has no leaf-script state capability.
        Reject([&] { slider->SetValue(80); });
        Reject([&] { Frame::Native::RequireDestroyable(); });
        if (fail) throw std::runtime_error("Authored native wheel failure");
        return 0;
    }
};
struct Guards {
    Gx::Native::GlContext& context;
    CSimpleTop& root;
    CSimpleScrollFrame* scroll;
    CSimpleSlider* slider;
    static int Call(lua_State* L) {
        auto& self = *static_cast<Guards*>(lua_touserdata(L, lua_upvalueindex(1)));
        Reject([&] { Frame::Native::DispatchInputEvent(self.context, OSEVENT{OS_INPUT_MOUSE_WHEEL,{-120,0,0,1}}); });
        Reject([&] { self.root.OnLayerUpdate(.016f); });
        Reject([&] { self.root.OnLayerRender(); });
        Reject([&] { Frame::Native::RequireDestroyable(); });
        Reject([&] { Frame::Native::RequireTreeMutation(); });
        Reject([&] { Frame::Native::ScrollRangeEdits edits; });
        Reject([&] { self.scroll->RunOnMouseWheelScript(1); });
        self.root.m_moveResizeFrame = self.slider;
        Reject([&] { self.scroll->Hide(); });
        self.root.m_moveResizeFrame = nullptr;
        Check(self.scroll->m_isShown && self.slider->m_isVisible, "Wheel movement preflight changed visibility");
        const auto flags = self.slider->m_frameFlags;
        self.slider->m_frameFlags |= 1;
        Reject([&] { self.slider->Show(); });
        self.slider->m_frameFlags = flags;
        Frame::Native::ResizeScope resize;
        Reject([&] { self.slider->SetValue(80); });
        Reject([&] { self.scroll->Hide(); });
        return 0;
    }
};
static void Run(CSimpleTop& root, Gx::Native::GlContext& context) {
    auto errors = FrameScript::Native::ScriptErrorCount();
    auto lua = [&](const char* source) {
        CStatus status;
        const auto result = FrameScript_ExecuteBuffer(source, std::strlen(source), "authored wheel controls", &status, nullptr);
        char detail[2048]{};
        if (!result || !status.IsEmpty() || FrameScript::Native::ScriptErrorCount() != errors)
            throw std::runtime_error(std::string("Wheel fixture: ") +
                status.GetErrorStr(detail, sizeof(detail), STATUS_WARNING, "; ") +
                FrameScript::Native::LastScriptError() + " source=" + source);
    };
    lua(R"(
        local f=CreateFrame('ScrollFrame','NativeWheel')
        f:SetSize(100,100); f:SetPoint('BOTTOMLEFT',nil,'BOTTOMLEFT',10,10)
        local c=CreateFrame('Frame',nil,f); c:SetSize(100,300); f:SetScrollChild(c)
        local s=CreateFrame('Slider','NativeWheelSlider',f)
        s:SetSize(16,100); s:SetPoint('LEFT',f,'RIGHT',0,0)
        s:SetValueStep(1); s:SetMinMaxValues(0,200); s:SetValue(0)
        local up=CreateFrame('Button','NativeWheelUp',s)
        local down=CreateFrame('Button','NativeWheelDown',s)
        up:Disable(); down:Enable()
        WheelOrder={}
        f:SetScript('OnVerticalScroll',function(self,value)
            table.insert(WheelOrder,'scroll'); s:SetValue(value)
            if value==0 then up:Disable() else up:Enable() end
            if value==200 then down:Disable() else down:Enable() end
        end)
        s:SetScript('OnValueChanged',function(self,value)
            table.insert(WheelOrder,'value'); f:SetVerticalScroll(value)
            assert(not pcall(self.SetWidth,self,20))
            assert(not pcall(f.SetScrollChild,f,c))
        end)
        f:SetScript('OnMouseWheel',function(self,delta)
            table.insert(WheelOrder,'wheel')
            NativeWheelGuards()
            s:SetValue(s:GetValue()-delta*50)
            s:Hide(); s:Show()
            assert(not pcall(self.SetParent,self,nil))
            assert(not pcall(self.EnableMouseWheel,self,false))
            assert(not pcall(self.SetFrameLevel,self,5))
            assert(not pcall(CreateFrame,'Frame','RejectedWheelFactory'))
            assert(not pcall(s.SetValue,{[0]=s[0]},50))
            assert(not pcall(s.SetValue,s,0/0))
            table.insert(WheelOrder,'tail')
        end)
        f:EnableMouseWheel(true)
    )");
    auto* scroll = static_cast<CSimpleScrollFrame*>(CScriptObject::GetScriptObjectByName("NativeWheel", CSimpleScrollFrame::GetObjectType()));
    auto* slider = static_cast<CSimpleSlider*>(CScriptObject::GetScriptObjectByName("NativeWheelSlider", CSimpleSlider::GetObjectType()));
    Guards guards{context, root, scroll, slider};
    auto* L = FrameScript_GetContext();
    lua_pushlightuserdata(L, &guards); lua_pushcclosure(L, Guards::Call, 1); lua_setglobal(L, "NativeWheelGuards");
    auto probe = std::make_unique<Probe>(slider);
    probe->SetAllPoints(scroll, 1); probe->SetFrameStrata(FRAME_STRATA_TOOLTIP);
    probe->EnableEvent(SIMPLE_EVENT_MOUSEWHEEL, 1);
    root.OnLayerUpdate(.016f);
    CRect rect{}; Check(scroll->GetRect(&rect), "Wheel hit rectangle missing");
    float x, y; DDCToNDC((rect.minX+rect.maxX)/2, (rect.minY+rect.maxY)/2, &x, &y);
    int width, height, pixelWidth, pixelHeight; context.Window().Size(width,height,pixelWidth,pixelHeight);
    Frame::Native::DispatchInputEvent(context, OSEVENT{OS_INPUT_MOUSE_MOVE,{0,int(x*width),int((1-y)*height),1}});
    auto deliver = [&](int amount) {
        Check(Frame::Native::DispatchInputEvent(context, OSEVENT{OS_INPUT_MOUSE_WHEEL,{amount,0,0,2}}) ==
            Frame::Native::InputResult::Handled, "Wheel callback did not consume input");
        Check(!Frame::Native::IsMouseWheelInput(), "Wheel dispatch pin leaked");
    };
    deliver(-240);
    lua("assert(table.concat(WheelOrder,',')=='wheel,value,scroll,tail'); "
        "assert(NativeWheelSlider:GetValue()==50 and NativeWheel:GetVerticalScroll()==50); "
        "assert(NativeWheelUp:IsEnabled()==1 and NativeWheelDown:IsEnabled()==1)");
    for (int i=0;i<5;++i) deliver(-120);
    lua("assert(NativeWheelSlider:GetValue()==200 and NativeWheel:GetVerticalScroll()==200); "
        "assert(NativeWheelDown:IsEnabled()==0 and NativeWheelUp:IsEnabled()==1)");
    for (int i=0;i<5;++i) deliver(120);
    lua("assert(NativeWheelSlider:GetValue()==0 and NativeWheel:GetVerticalScroll()==0); "
        "assert(NativeWheelUp:IsEnabled()==0 and NativeWheelDown:IsEnabled()==1)");
    probe->fail = true;
    bool failed = false; try { deliver(-120); } catch (const std::runtime_error&) { failed = true; }
    Check(failed && !Frame::Native::IsMouseWheelInput(), "Native wheel exception retained dispatch pin");
    probe->fail = false;
    deliver(-120);
    lua("NativeWheel:HookScript('OnMouseWheel',function() error('authored protected wheel failure') end)");
    deliver(-120);
    Check(FrameScript::Native::ScriptErrorCount() == errors+1 &&
        FrameScript::Native::LastScriptError().find("authored protected wheel failure") != std::string::npos,
        "Expected protected wheel error was not retained");
    ++errors;
    lua("NativeWheel:SetScript('OnMouseWheel',nil); NativeWheelSlider:SetValue(0)");
    { Frame::Native::CallbackScope callback; Reject([&] { slider->SetValue(10); }); }
    { Frame::Native::CollectScope collect; Reject([&] { deliver(-120); }); }
    { Frame::Native::ResizeScope resize; Reject([&] { deliver(-120); }); }
    {
        CRenderBatch batch;
        batch.QueueCallback(+[](void* raw) {
            auto& self = *static_cast<Guards*>(raw);
            Reject([&] { Frame::Native::DispatchInputEvent(self.context, OSEVENT{OS_INPUT_MOUSE_WHEEL,{-120,0,0,3}}); });
        }, &guards);
        batch.Finish(); CSimpleRender::DrawBatch(&batch);
    }
    bool foreign = false;
    std::thread thread([&] { try { deliver(-120); } catch (const std::logic_error&) { foreign = true; } });
    thread.join(); Check(foreign, "Foreign wheel dispatch admitted");
    Check(probe->calls >= 14, "Wheel priority consumer was skipped");
    probe.reset(); delete scroll;
    lua("NativeWheelGuards,WheelOrder=nil,nil; collectgarbage('collect')");
    std::cout << "native-wheel-dispatch-slider-scroll-order-clamp-reentry-guards-recovery=passed\n";
}
}
