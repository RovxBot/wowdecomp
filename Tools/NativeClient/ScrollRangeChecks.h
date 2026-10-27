// Authored existing-control sequence corresponding to the reached original
// scroll-range callback. No game Lua/XML is copied into this fixture.
#include "Engine/Source/Frame/CSimpleScrollFrame.h"
#include "Engine/Source/Frame/CSimpleSlider.h"
#include "Engine/Source/Frame/CRenderBatch.h"
#include "Engine/Source/Frame/CSimpleRender.h"
#include <thread>
static void ScrollCheck(bool good, const char* message) { if (!good) throw std::runtime_error(message); }
static void CheckScrollRangeEdits(CSimpleTop& root) {
    const auto errors = FrameScript::Native::ScriptErrorCount();
    auto lua = [&](const char* source) {
        if (FrameScript::Native::ScriptErrorCount() != errors)
            throw std::runtime_error(std::string("Scroll-range protected callback: ") + FrameScript::Native::LastScriptError());
        CStatus status;
        const auto result = FrameScript_ExecuteBuffer(source, std::strlen(source), "authored scroll-range controls", &status, nullptr);
        if (!result || !status.IsEmpty()) {
            char error[2048]{};
            throw std::runtime_error(std::string("Scroll-range fixture: ") + status.GetErrorStr(error, sizeof(error), STATUS_WARNING, "; ") + " source=" + source);
        }
        ScrollCheck(FrameScript::Native::ScriptErrorCount() == errors, "Scroll-range callback failed");
    };
    lua(R"(
        local f=CreateFrame('ScrollFrame','NativeRange')
        f:SetSize(100,100); f:SetPoint('BOTTOMLEFT',nil,'BOTTOMLEFT',10,10)
        local c=CreateFrame('Frame','NativeRangeContent',f)
        c:SetSize(100,240); f:SetScrollChild(c)
        local s=CreateFrame('Slider','NativeRangeSlider',f)
        s:SetSize(16,100); s:SetPoint('LEFT',f,'RIGHT',0,0)
        local t=s:CreateTexture(); t:SetTexture(0,1,0,1); t:SetSize(8,8); s:SetThumbTexture(t)
        s:SetMinMaxValues(0,200); s:SetValue(100); f:SetVerticalScroll(100)
        local up=CreateFrame('Button','NativeRangeUp',s)
        local down=CreateFrame('Button','NativeRangeDown',s)
        up:Disable(); down:Disable()
        RangeCalls,RangeValues,RangeTail=0,0,0
        local tail=CreateFrame('Frame','NativeRangeTail')
        tail:SetScript('OnUpdate',function() RangeTail=RangeTail+1 end)
        s:SetScript('OnValueChanged',function(self,value)
            RangeValues=RangeValues+1
            f:SetVerticalScroll(value)
            assert(not pcall(f.SetWidth,f,200))
            assert(not pcall(CreateFrame,'Frame','RejectedRangeNested'))
        end)
        f:SetScript('OnScrollRangeChanged',function(self,x,y)
            s:SetMinMaxValues(0,y)
            s:SetValue(math.min(s:GetValue(),y))
            if y<1 then
                s:Hide(); up:Hide(); down:Hide(); up:Disable(); down:Disable()
            else
                s:Show(); up:Show(); down:Show(); up:Enable(); down:Enable()
            end
            assert(not pcall(self.SetParent,self,nil))
            assert(not pcall(self.SetWidth,self,200))
            assert(not pcall(self.SetFrameLevel,self,5))
            assert(not pcall(self.EnableMouse,self,true))
            assert(not pcall(CreateFrame,'Frame','RejectedRangeFactory'))
            assert(not pcall(s.SetMinMaxValues,s,-3e38,3e38))
            RangeCalls=RangeCalls+1
        end)
    )");
    root.OnLayerUpdate(.016f);
    lua("assert(RangeCalls==1 and NativeRangeSlider:IsShown() and NativeRangeUp:IsEnabled() and NativeRangeDown:IsEnabled()); "
        // Original Slider LoadXML selects a .001 step, whose float snap can
        // produce 100.000008. Preserve that arithmetic; compare within a step.
        "assert(math.abs(NativeRangeSlider:GetValue()-100)<.001 and RangeTail==1)");
    lua("NativeRangeContent:SetHeight(40)");
    root.OnLayerUpdate(.016f);
    root.OnLayerUpdate(.016f);
    lua(R"(
        local s=NativeRangeSlider
        local lo,hi=s:GetMinMaxValues()
        local state=string.format('calls=%s values=%s tail=%s range=%g,%g value=%g scroll=%g shown=%s up=%s down=%s',
            tostring(RangeCalls),tostring(RangeValues),tostring(RangeTail),lo,hi,s:GetValue(),NativeRange:GetVerticalScroll(),
            tostring(s:IsShown()),tostring(NativeRangeUp:IsEnabled()),tostring(NativeRangeDown:IsEnabled()))
        assert(RangeCalls==2 and RangeValues==1 and s:GetValue()==0 and NativeRange:GetVerticalScroll()==0,state)
        assert(not s:IsShown() and not NativeRangeUp:IsShown() and not NativeRangeDown:IsShown(),state)
        assert(not NativeRangeUp:IsEnabled() and not NativeRangeDown:IsEnabled() and RangeTail==3,state)
    )");
    lua("NativeRangeContent:SetHeight(260)");
    root.OnLayerUpdate(.016f);
    root.OnLayerUpdate(.016f);
    lua("assert(RangeCalls==3 and NativeRangeSlider:IsVisible() and NativeRangeUp:IsVisible() and NativeRangeDown:IsVisible()); "
        "assert(NativeRangeUp:IsEnabled() and NativeRangeDown:IsEnabled() and RangeTail==5)");
    auto* scroll = static_cast<CSimpleScrollFrame*>(CScriptObject::GetScriptObjectByName("NativeRange", CSimpleScrollFrame::GetObjectType()));
    auto* slider = static_cast<CSimpleSlider*>(CScriptObject::GetScriptObjectByName("NativeRangeSlider", CSimpleSlider::GetObjectType()));
    auto rejected = [](auto action) {
        bool failed = false; try { action(); } catch (const std::logic_error&) { failed = true; }
        ScrollCheck(failed, "Unsafe scroll-range operation admitted");
    };
    {
        Frame::Native::CallbackScope callback;
        rejected([&] { slider->SetValue(10); });
        rejected([&] { slider->SetMinMaxValues(0, 10); });
        rejected([&] { scroll->Hide(); });
    }
    {
        Frame::Native::CollectScope collect;
        rejected([&] { Frame::Native::ScrollRangeEdits edits; });
    }
    {
        Frame::Native::ScrollRangeEdits edits;
        Frame::Native::CallbackScope callback;
        rejected([&] { Frame::Native::RequireDestroyable(); });
        rejected([&] { Frame::Native::RequireTreeMutable(); });
        root.m_moveResizeFrame = slider;
        rejected([&] { scroll->Hide(); });
        root.m_moveResizeFrame = nullptr;
        ScrollCheck(scroll->m_isShown && slider->m_isVisible, "Rejected movement visibility edit mutated the subtree");
        Frame::Native::ResizeScope resize;
        rejected([&] { slider->SetValue(10); });
        rejected([&] { scroll->Hide(); });
    }
    bool foreign = false;
    std::thread thread([&] { try { Frame::Native::ScrollRangeEdits edits; } catch (const std::logic_error&) { foreign = true; } });
    thread.join(); ScrollCheck(foreign, "Foreign scroll-range capability admitted");
    {
        CRenderBatch batch;
        bool called = false;
        batch.QueueCallback(+[](void* raw) {
            *static_cast<bool*>(raw) = true;
            bool failed = false;
            try { Frame::Native::ScrollRangeEdits edits; } catch (const std::logic_error&) { failed = true; }
            ScrollCheck(failed, "Render-time scroll-range capability admitted");
        }, &called);
        batch.Finish(); CSimpleRender::DrawBatch(&batch);
        ScrollCheck(called, "Scroll-range render guard fixture did not run");
    }
    lua("StaleRangeSlider=NativeRangeSlider");
    delete scroll;
    delete static_cast<CSimpleFrame*>(CScriptObject::GetScriptObjectByName("NativeRangeTail", CSimpleFrame::GetObjectType()));
    lua("assert(not pcall(StaleRangeSlider.SetValue,StaleRangeSlider,10)); "
        "StaleRangeSlider,RangeCalls,RangeValues,RangeTail=nil,nil,nil,nil; collectgarbage('collect')");
    std::cout << "native-scroll-range-existing-controls-order-clamp-visibility-guards=passed\n";
}
