// Authored agreement-shaped transition; never invokes an original agreement API.
#include "Engine/Source/Frame/Portable/WindowEvents.h"
namespace ButtonClickChecks {
static void Check(bool good, const char* message) { if (!good) throw std::runtime_error(message); }
template<class F> static void Reject(F action) {
    bool failed = false;
    try { action(); } catch (const std::logic_error&) { failed = true; }
    Check(failed, "Unsafe Button operation admitted");
}
struct Guards {
    Gx::Native::GlContext& context;
    CSimpleTop& root;
    CSimpleFrame* panel;
    CSimpleButton* button;
    static int Call(lua_State* L) {
        auto& self = *static_cast<Guards*>(lua_touserdata(L, lua_upvalueindex(1)));
        Reject([&] { Frame::Native::DispatchInputEvent(self.context, OSEVENT{OS_INPUT_MOUSE_UP,{MOUSE_BUTTON_LEFT,0,0,1}}); });
        Reject([&] { self.root.OnLayerUpdate(.016f); });
        Reject([&] { self.root.OnLayerRender(); });
        Reject([&] { Frame::Native::RequireDestroyable(); });
        Reject([&] { Frame::Native::RequireTreeMutation(); });
        Reject([&] { Frame::Native::ScrollRangeEdits edits; });
        self.root.m_moveResizeFrame = self.button;
        Reject([&] { self.panel->Hide(); });
        self.root.m_moveResizeFrame = nullptr;
        Check(self.panel->m_isShown && self.button->m_isVisible, "Button movement preflight changed visibility");
        const auto flags = self.button->m_frameFlags;
        self.button->m_frameFlags |= 1;
        Reject([&] { self.panel->Show(); });
        self.button->m_frameFlags = flags;
        Frame::Native::ResizeScope resize;
        Reject([&] { self.panel->Hide(); });
        return 0;
    }
};
struct ThrowButton final : CSimpleButton {
    ThrowButton() : CSimpleButton(nullptr) {}
    int32_t OnLayerMouseUp(const CMouseEvent&, const char*) override {
        Reject([&] { Hide(); }); // An arbitrary virtual consumer has no capability.
        throw std::runtime_error("Authored native button delivery failure");
    }
};
static void Run(CSimpleTop& root, Gx::Native::GlContext& context) {
    auto errors = FrameScript::Native::ScriptErrorCount();
    auto lua = [&](const char* source) {
        CStatus status;
        const auto result = FrameScript_ExecuteBuffer(source, std::strlen(source), "authored button controls", &status, nullptr);
        char detail[2048]{};
        if (!result || !status.IsEmpty() || FrameScript::Native::ScriptErrorCount() != errors)
            throw std::runtime_error(std::string("Button fixture: ") +
                status.GetErrorStr(detail, sizeof(detail), STATUS_WARNING, "; ") +
                FrameScript::Native::LastScriptError() + " source=" + source);
    };
    lua(R"(
        local p=CreateFrame('Frame','NativeClickPanel')
        p:SetSize(100,100); p:SetPoint('BOTTOMLEFT',nil,'BOTTOMLEFT',10,10)
        local b=CreateFrame('Button','NativeClick',p)
        b:SetAllPoints(p)
        local n=CreateFrame('Frame','NativeClickNext'); n:Hide()
        local other=CreateFrame('Button','NativeClickOther',n)
        ClickOrder={}
        p:SetScript('OnHide',function() table.insert(ClickOrder,'panel-hide') end)
        b:SetScript('OnHide',function() table.insert(ClickOrder,'button-hide') end)
        b:SetScript('OnLeave',function()
            table.insert(ClickOrder,'leave')
            assert(not pcall(n.Show,n)) -- Cursor retirement keeps its resize guard.
            assert(not pcall(other.Disable,other))
            assert(not pcall(other.Click,other))
        end)
        b:SetScript('OnMouseDown',function(self)
            assert(not pcall(p.Hide,p)); assert(not pcall(self.Click,self))
        end)
        b:SetScript('OnMouseUp',function(self)
            assert(not pcall(p.Hide,p)); assert(not pcall(self.Click,self))
        end)
        b:SetScript('PreClick',function(self,key,down)
            assert(self==NativeClick and key=='LeftButton')
            NativeClickGuards(); table.insert(ClickOrder,'pre')
        end)
        b:SetScript('OnClick',function(self,key,down)
            table.insert(ClickOrder,'click'); self:Click(key) -- Same-button recursion is suppressed.
            assert(not pcall(other.Click,other))
            assert(not pcall(self.SetWidth,self,20))
            assert(not pcall(self.SetParent,self,nil))
            assert(not pcall(self.EnableMouse,self,false))
            assert(not pcall(self.SetFrameLevel,self,5))
            assert(not pcall(CreateFrame,'Frame','RejectedClickFactory'))
            assert(not pcall(p.Hide,{[0]=p[0]}))
            p:Hide(); self:Disable(); n:Show()
            table.insert(ClickOrder,'tail')
        end)
        b:SetScript('PostClick',function() table.insert(ClickOrder,'post') end)
        n:SetScript('OnShow',function()
            other:Disable(); other:Enable(); table.insert(ClickOrder,'next-show')
        end)
    )");
    auto* panel = static_cast<CSimpleFrame*>(CScriptObject::GetScriptObjectByName("NativeClickPanel", CSimpleFrame::GetObjectType()));
    auto* button = static_cast<CSimpleButton*>(CScriptObject::GetScriptObjectByName("NativeClick", CSimpleButton::GetObjectType()));
    auto* next = static_cast<CSimpleFrame*>(CScriptObject::GetScriptObjectByName("NativeClickNext", CSimpleFrame::GetObjectType()));
    Guards guards{context, root, panel, button};
    auto* L = FrameScript_GetContext();
    lua_pushlightuserdata(L, &guards); lua_pushcclosure(L, Guards::Call, 1); lua_setglobal(L, "NativeClickGuards");
    root.OnLayerUpdate(.016f);
    CRect rect{}; Check(button->GetRect(&rect), "Button hit rectangle missing");
    float x, y; DDCToNDC((rect.minX+rect.maxX)/2, (rect.minY+rect.maxY)/2, &x, &y);
    int width, height, pixelWidth, pixelHeight; context.Window().Size(width,height,pixelWidth,pixelHeight);
    auto deliver = [&](OSINPUT id, int key=0) {
        return Frame::Native::DispatchInputEvent(context, OSEVENT{id,{key,int(x*width),int((1-y)*height),2}});
    };
    auto released = [&] {
        Check(!Frame::Native::IsMouseButtonInput() && !root.m_mouseCapture && !root.m_mouseFocus &&
            !root.m_nativeButtonsDown && !button->m_isMouseDown && !button->m_isMouseDragging &&
            !button->m_inClick && !root.m_mouseButtonName && !root.unused_1250,
            "Button transition retained input/callback state");
    };
    for (bool down : {false, true}) {
        if (down) lua("NativeClickPanel:Show(); NativeClick:Enable(); NativeClickNext:Hide(); NativeClick:RegisterForClicks('LeftButtonDown'); ClickOrder={}");
        deliver(OS_INPUT_MOUSE_MOVE);
        Check(root.m_mouseFocus == button, "Button did not receive cursor focus");
        deliver(OS_INPUT_MOUSE_DOWN, MOUSE_BUTTON_LEFT);
        deliver(OS_INPUT_MOUSE_UP, MOUSE_BUTTON_LEFT);
        lua("assert(table.concat(ClickOrder,',')=='pre,click,leave,button-hide,panel-hide,next-show,tail,post'); "
            "assert(not NativeClickPanel:IsShown() and NativeClickNext:IsShown() and NativeClick:IsEnabled()==0)");
        released();
    }
    // Direct Click keeps ordinary admission, including after the input scope ends.
    lua("NativeClick:SetScript('PreClick',nil); NativeClick:SetScript('PostClick',nil); NativeClick:Enable(); "
        "NativeClick:SetScript('OnClick',function(self) assert(not pcall(self.Hide,self)) end); NativeClick:Click()");
    // A protected error still restores the click identity and dispatch pin.
    lua("NativeClickPanel:Show(); NativeClick:RegisterForClicks('LeftButtonUp'); "
        "NativeClick:SetScript('OnClick',function() error('authored protected button failure') end)");
    deliver(OS_INPUT_MOUSE_MOVE); deliver(OS_INPUT_MOUSE_DOWN, MOUSE_BUTTON_LEFT); deliver(OS_INPUT_MOUSE_UP, MOUSE_BUTTON_LEFT);
    Check(FrameScript::Native::ScriptErrorCount()==errors+1 &&
        FrameScript::Native::LastScriptError().find("authored protected button failure")!=std::string::npos,
        "Expected protected Button error was not retained");
    ++errors;
    Check(!Frame::Native::IsMouseButtonInput() && !button->m_inClick && !root.m_mouseCapture &&
        !root.m_mouseButtonName && !root.unused_1250, "Protected Button failure retained callback state");
    lua("NativeClick:SetScript('OnClick',nil)");
    // Disable/enable while focused also invokes cursor scripts, outside HideFrame.
    lua("ClickOrder={}; NativeClick:SetScript('OnClick',function(self) self:Disable(); self:Enable() end)");
    deliver(OS_INPUT_MOUSE_DOWN,MOUSE_BUTTON_LEFT); deliver(OS_INPUT_MOUSE_UP,MOUSE_BUTTON_LEFT);
    lua("assert(table.concat(ClickOrder,',')=='leave'); NativeClick:SetScript('OnClick',nil)");
    { Frame::Native::CallbackScope callback; Reject([&] { panel->Hide(); }); }
    { Frame::Native::CollectScope collect; Reject([&] { deliver(OS_INPUT_MOUSE_UP,MOUSE_BUTTON_LEFT); }); }
    { Frame::Native::ResizeScope resize; Reject([&] { deliver(OS_INPUT_MOUSE_UP,MOUSE_BUTTON_LEFT); }); }
    {
        CRenderBatch batch;
        batch.QueueCallback(+[](void* raw) {
            auto& self = *static_cast<Guards*>(raw);
            Reject([&] { Frame::Native::DispatchInputEvent(self.context, OSEVENT{OS_INPUT_MOUSE_UP,{MOUSE_BUTTON_LEFT,0,0,3}}); });
        }, &guards);
        batch.Finish(); CSimpleRender::DrawBatch(&batch);
    }
    bool foreign=false;
    std::thread thread([&] { try { deliver(OS_INPUT_MOUSE_UP,MOUSE_BUTTON_LEFT); } catch (const std::logic_error&) { foreign=true; } });
    thread.join(); Check(foreign,"Foreign Button input admitted");
    lua("NativeClick:SetScript('OnLeave',nil)");
    root.CancelMouseInput(); delete panel; delete next;
    // Native virtual failure unwinds the input pin; a subsequent ordinary input works.
    {
        auto failing=std::make_unique<ThrowButton>();
        root.m_mouseCapture=failing.get();
        bool failed=false;
        try { deliver(OS_INPUT_MOUSE_UP,MOUSE_BUTTON_LEFT); } catch (const std::runtime_error&) { failed=true; }
        Check(failed && !Frame::Native::IsMouseButtonInput(),"Native Button failure retained input pin");
        root.m_mouseCapture=nullptr;
        deliver(OS_INPUT_MOUSE_UP,MOUSE_BUTTON_LEFT);
    }
    lua("NativeClickGuards,ClickOrder=nil,nil; collectgarbage('collect')");
    std::cout<<"native-button-dispatch-visibility-capture-order-reentry-guards-recovery=passed\n";
}
}
