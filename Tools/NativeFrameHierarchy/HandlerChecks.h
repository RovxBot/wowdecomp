// Authored original Frame/Lua/XML consumers, separate from signed GlueXML or
// physical input acceptance. Runs in each complete hierarchy owner cycle.
#include "Engine/Source/Event/CCharEvent.h"
#include "Engine/Source/Event/CKeyEvent.h"
static void HandlerChecks(GlContext& context, LegacyDevice& device, CSimpleTop& root, CSimpleFont& font) {
    auto* frame = Create_SimpleFrame(nullptr);
    frame->SetName("NativeHandlerFrame");
    frame->SetSize(.7f, .4f);
    frame->SetPoint(FRAMEPOINT_BOTTOMLEFT, &root, FRAMEPOINT_BOTTOMLEFT, .05f, .05f, 1);
    auto* text = Frame::Native::CreateFontString(frame, 2, true);
    text->SetName("NativeHandlerText"); text->SetAllPoints(frame, 1); text->SetFontObject(&font);
    text->SetText("A", 0);
    const auto initialPixels=Draw(device,root);
    lua_State* L = FrameScript_GetContext();
    const int top = lua_gettop(L), nesting = lua_taintexpected;
    int previousErrorHandler=LUA_NOREF;
    if (s_errorHandlerFunc>0) {
        lua_rawgeti(L,LUA_REGISTRYINDEX,s_errorHandlerFunc);
        previousErrorHandler=luaL_ref(L,LUA_REGISTRYINDEX);
    }
    const char* names[] = {"NATIVE_HANDLER_EVENT", "NATIVE_HANDLER_OTHER", nullptr};
    FrameScript_CreateEvents(names, 3);
    RegionLua(R"(
        HandlerLog,HandlerErrors={},{}
        seterrorhandler(function(err) HandlerErrors[#HandlerErrors+1]=err; return err end)
        local f,t = NativeHandlerFrame,NativeHandlerText
        assert(f:HasScript('OnKeyDown') and f:HasScript('OnLoad') and f:HasScript('OnEvent'))
        assert(not f:HasScript('OnClick') and f:GetScript('OnUpdate') == nil)
        assert(not pcall(f.GetScript,f,'OnClick'))
        assert(not pcall(f.SetScript,f,'OnClick',function() end))
        assert(not pcall(f.SetScript,f,'OnUpdate',true))
        assert(not pcall(f.HookScript,f,'OnUpdate',nil))
        assert(not pcall(f.SetScript,{[0]=f[0]},'OnLoad',function() end))
        local first = function(self,key)
            assert(self==f and self==this and key=='A' and arg1=='A')
            assert(self:GetWidth()>=0 and self:GetHeight()>=0 and self:GetNumPoints()>=0)
            assert(not pcall(self.Hide,self) and not pcall(CreateFrame,'Frame','RejectedCallback'))
            t:SetText('V'); assert(t:GetText()=='V')
            HandlerLog[#HandlerLog+1]='key'
        end
        f:SetScript('OnKeyDown',first,'ignored trailing argument')
        assert(f:GetScript('onkeydown')==first)
        f:HookScript('OnKeyDown',function(self,key)
            assert(self==f and key=='A'); HandlerLog[#HandlerLog+1]='hook'
        end, function() error('wrong trailing argument retained') end)
        f:HookScript('OnKeyDown',function(self,key)
            HandlerLog[#HandlerLog+1]='last'
            self:SetScript('OnKeyDown',nil)
        end)
        f:SetScript('OnKeyUp',function(self,key) HandlerLog[#HandlerLog+1]='up:'..key end)
        f:SetScript('OnChar',function(self,ch)
            assert(self==f and ch=='😀'); HandlerLog[#HandlerLog+1]='char'
        end)
        f:EnableKeyboard(true); assert(f:IsKeyboardEnabled())
        f:EnableMouse(true); f:EnableMouseWheel(true)
        assert(f:IsMouseEnabled() and f:IsMouseWheelEnabled())
        f:SetScript('OnMouseDown',function(self,button)
            assert(button=='RightButton'); HandlerLog[#HandlerLog+1]='down'
        end)
        f:SetScript('OnMouseUp',function(self,button)
            assert(button=='RightButton'); HandlerLog[#HandlerLog+1]='up'
        end)
        f:SetScript('OnMouseWheel',function(self,delta)
            assert(delta==-1); HandlerLog[#HandlerLog+1]='wheel'
        end)
        f:SetScript('OnEnter',function(self,motion) assert(motion); HandlerLog[#HandlerLog+1]='enter' end)
        f:SetScript('OnLeave',function(self,motion) assert(not motion); HandlerLog[#HandlerLog+1]='leave' end)
        f:SetScript('OnDragStart',function(self,button) assert(button=='RightButton'); HandlerLog[#HandlerLog+1]='drag' end)
        f:SetScript('OnDragStop',function(self) HandlerLog[#HandlerLog+1]='stop' end)
        f:SetScript('OnReceiveDrag',function(self) HandlerLog[#HandlerLog+1]='receive' end)
        this,arg1='saved-this','saved-arg'
    )");
    // Original Glue dropdowns construct buttons from OnLoad. A checked factory
    // may build/unwind new objects there; callbacks still pin their own tree.
    CStatus loadStatus;
    const char* loadTemplates=R"(<Ui>
      <Frame name='NativeLoadFactoryTemplate' virtual='true'><Scripts><OnLoad>
        assert(not pcall(self.SetParent,self,nil)); self.constructorPinned=true
      </OnLoad></Scripts></Frame>
      <Frame name='NativeLoadFailureTemplate' virtual='true' frameLevel='129'/>
    </Ui>)";
    std::unique_ptr<XMLTree,decltype(&XMLTree_Free)> loadTree(XMLTree_Load(loadTemplates,std::strlen(loadTemplates)),&XMLTree_Free);
    auto* loadNode=XMLTree_GetRoot(loadTree.get())->m_child;
    FrameXML_StoreHashNode(loadNode,"NativeLoadFactoryTemplate",nullptr,&loadStatus);
    FrameXML_StoreHashNode(XMLTree_GetRoot(loadTree.get())->m_child,"NativeLoadFailureTemplate",nullptr,&loadStatus);
    loadTree.reset();
    const auto beforeLoadFrames=root.m_frameCount;
    RegionLua(R"(
        NativeHandlerFrame:SetScript('OnLoad',function(self)
            self:SetWidth(self:GetWidth())
            self:Hide(); assert(not self:IsShown()); self:Show()
            assert(not pcall(self.SetParent,self,nil))
            assert(not pcall(CreateFrame,'Frame','FailedLoadFactory',self,'NativeLoadFailureTemplate'))
            assert(FailedLoadFactory==nil)
            local child=CreateFrame('Frame','CreatedLoadCallback',self,'NativeLoadFactoryTemplate')
            assert(child.constructorPinned)
            child:SetID(7); assert(child:GetID()==7)
            assert(not pcall(child.SetID,child,2^40))
            HandlerLog[#HandlerLog+1]='load-state'
        end)
    )");
    {
        Frame::Native::LoadScriptEdits edits;
        frame->RunScript(frame->m_onLoad,0,nullptr);
        Frame::Native::CallbackScope pinned;
        Reject([&]{Frame::Native::RequireDestroyable();});
    }
    RegionLua("assert(HandlerLog[#HandlerLog]=='load-state' and #HandlerErrors==0); NativeHandlerFrame:SetScript('OnLoad',nil); HandlerLog={}");
    Check(root.m_frameCount==beforeLoadFrames+1,"Failed callback factory stranded or retired a Frame");
    delete static_cast<CSimpleFrame*>(CScriptObject::GetScriptObjectByName("CreatedLoadCallback",CSimpleFrame::GetObjectType()));
    // Use actual root key/character queues, with the same adapter used after SDL
    // pumping. A handled key captures the Frame until release.
    Check(Frame::Native::DispatchInputEvent(context, OSEVENT{OS_INPUT_KEY_DOWN,{KEY_A,1,0,10}}) ==
          Frame::Native::InputResult::Handled, "Lua key handler did not consume original root queue");
    Check(Frame::Native::DispatchInputEvent(context, OSEVENT{OS_INPUT_KEY_UP,{KEY_A,1,0,11}}) ==
          Frame::Native::InputResult::Handled, "Lua key release did not reach capture");
    Check(Frame::Native::DispatchInputEvent(context, OSEVENT{OS_INPUT_CHAR,{0x1f600,1,0,12}}) ==
          Frame::Native::InputResult::Handled, "Lua Unicode character did not reach root queue");
    CMouseEvent mouse; mouse.m_data.m_button=0x4; mouse.m_data.m_buttonState=0x4;
    mouse.m_data.m_mousePosition={.1f,.1f}; mouse.m_data.m_wheelAmount=uint32_t(-2);
    root.m_mouseButtonName="saved-button";
    frame->OnLayerMouseDown(mouse,nullptr); frame->OnLayerMouseUp(mouse,nullptr);
    Check(root.m_mouseButtonName && !std::strcmp(root.m_mouseButtonName,"saved-button"), "Mouse button borrow not restored");
    frame->OnLayerMouseWheel(mouse); frame->RunOnEnterScript(1); frame->RunOnLeaveScript(0);
    frame->OnDragStart(mouse); frame->OnDragStop(mouse); frame->OnReceiveDrag(mouse);
    root.m_mouseButtonName=nullptr;
    const auto keyPixels=Draw(device,root);
    Check(std::memcmp(initialPixels.data(),keyPixels.data(),keyPixels.size()*sizeof(CImVector))!=0,
          "Lua key callback did not change glyph pixels");
    RegionLua(R"(
        assert(table.concat(HandlerLog,',')=='key,hook,last,up:A,char,down,up,wheel,enter,leave,drag,stop,receive')
        assert(this=='saved-this' and arg1=='saved-arg' and #HandlerErrors==0)
        local f,t=NativeHandlerFrame,NativeHandlerText
        assert(f:GetScript('OnKeyDown')==nil)
        f:SetScript('OnUpdate',function(self,elapsed)
            assert(elapsed>0 and elapsed<1)
            t:SetText('AV callback')
            self:SetScript('OnUpdate',nil)
            HandlerLog[#HandlerLog+1]='update'
        end)
        f:SetScript('OnHide',function(self) HandlerLog[#HandlerLog+1]='hide' end)
        f:SetScript('OnShow',function(self) HandlerLog[#HandlerLog+1]='show' end)
        f:Hide(); f:Show()
    )");
    auto pixels=Draw(device,root);
    Check(Ink(pixels)>4,"Lua update handler did not render changed FontString");
    Check(std::memcmp(keyPixels.data(),pixels.data(),pixels.size()*sizeof(CImVector))!=0,
          "Lua update callback did not change glyph pixels");
    Equal(pixels,Draw(device,root));
    RegionLua("assert(NativeHandlerText:GetText()=='AV callback' and HandlerLog[#HandlerLog]=='update' and #HandlerErrors==0)");
    // Events retain original listener deferral and can replace their own handler.
    RegionLua(R"(
        HandlerEvents={}
        local f=NativeHandlerFrame
        f:SetScript('OnEvent',function(self,eventName,payload)
            assert(self==f and self==this and eventName==event and payload==arg1)
            HandlerEvents[#HandlerEvents+1]=eventName..':'..payload
            self:UnregisterEvent(eventName)
            self:SetScript('OnEvent',function() error('retired event listener ran') end)
        end)
        f:RegisterEvent('NATIVE_HANDLER_EVENT'); f:RegisterEvent('NATIVE_HANDLER_EVENT')
        assert(f:IsEventRegistered('NATIVE_HANDLER_EVENT') and not f:IsEventRegistered('ABSENT'))
    )");
    FrameScript_SignalEvent(0,"%s","payload"); FrameScript_SignalEvent(0,"%s","ignored");
    RegionLua(R"(
        local f=NativeHandlerFrame
        assert(#HandlerEvents==1 and HandlerEvents[1]=='NATIVE_HANDLER_EVENT:payload' and #HandlerErrors==0)
        f:RegisterAllEvents(); assert(f:IsEventRegistered('NATIVE_HANDLER_OTHER'))
        f:UnregisterAllEvents(); assert(not f:IsEventRegistered('NATIVE_HANDLER_OTHER'))
        f:SetScript('OnUpdate',function() error('authored handler failure') end)
    )");
    root.OnLayerUpdate(.016f);
    RegionLua(R"(
        assert(#HandlerErrors==1 and string.find(HandlerErrors[1],'authored handler failure'))
        NativeHandlerFrame:SetScript('OnUpdate',nil)
        -- Internal compatibility globals must never invoke _G assignment hooks
        -- before or after the protected callback.
        HandlerMetaCalls=0
        HandlerOldGlobalMeta=getmetatable(_G)
        setmetatable(_G,{__newindex=function() HandlerMetaCalls=HandlerMetaCalls+1; error('unexpected global hook') end})
        rawset(_G,'this',nil); rawset(_G,'arg1',nil)
    )");
    frame->RunOnEnterScript(1);
    RegionLua(R"(
        setmetatable(_G,HandlerOldGlobalMeta)
        assert(HandlerMetaCalls==0 and this==nil and arg1==nil and #HandlerErrors==1)
        NativeHandlerFrame:SetScript('OnEnter',nil)
    )");
    // Original callback taint is retained, separate from the callback's value.
    FrameScript_Execute("NativeHandlerFrame:SetScript('OnLoad',function(self) HandlerTaintCalled=true; HandlerTaintSecure=issecure() end)","@authored-handler-addon","HandlerAddon");
    Check(frame->m_onLoad.m_tainted && !std::strcmp(frame->m_onLoad.m_tainted,"HandlerAddon"),"Frame handler taint was discarded");
    frame->RunScript(frame->m_onLoad,0,nullptr);
    // Original issecure returns nil, not Lua false, for tainted execution.
    RegionLua("assert(HandlerTaintCalled and HandlerTaintSecure==nil)");
    // XML compiles and retains an original wrapper in a transferred virtual node.
    CStatus status;
    const char* document=R"(<Ui><Frame name="NativeHandlerTemplate" virtual="true"><Scripts>
      <OnLoad>assert(self==this); self.loads=(self.loads or 0)+1; self:RegisterEvent('NATIVE_HANDLER_EVENT')</OnLoad>
      <OnUpdate function="NativeXMLUpdate"/>
      <OnChar>self.lastChar=text</OnChar>
    </Scripts></Frame></Ui>)";
    std::unique_ptr<XMLTree,decltype(&XMLTree_Free)> tree(XMLTree_Load(document,std::strlen(document)),&XMLTree_Free);
    Check(bool(tree),"Handler XML parse");
    FrameXML_StoreHashNode(XMLTree_GetRoot(tree.get())->m_child,"NativeHandlerTemplate",nullptr,&status); tree.reset();
    RegionLua(R"(
        NativeXMLUpdate=function(self,elapsed) assert(elapsed>0); self.ticks=(self.ticks or 0)+1 end
        HandlerXML1=CreateFrame('Frame','HandlerXML1',nil,'NativeHandlerTemplate')
        HandlerXML2=CreateFrame('Frame','HandlerXML2',nil,'NativeHandlerTemplate')
        assert(HandlerXML1.loads==1 and HandlerXML2.loads==1)
        assert(HandlerXML1:GetScript('OnLoad')==HandlerXML2:GetScript('OnLoad'))
        assert(HandlerXML1:GetScript('OnUpdate')==NativeXMLUpdate)
        assert(HandlerXML1:IsEventRegistered('NATIVE_HANDLER_EVENT'))

    )");
    auto* xml1=static_cast<CSimpleFrame*>(CScriptObject::GetScriptObjectByName("HandlerXML1",CSimpleFrame::GetObjectType()));
    auto* xml2=static_cast<CSimpleFrame*>(CScriptObject::GetScriptObjectByName("HandlerXML2",CSimpleFrame::GetObjectType()));
    FrameXML_FreeHashNodes(); // Live Frame slots independently retain compiled closures.
    root.OnLayerUpdate(.016f);
    RegionLua("assert(HandlerXML1.ticks==1 and HandlerXML2.ticks==1 and #HandlerErrors==1)");
    for (const char* body : {"<OnClick/>","<OnLoad inherit='append'/>","<OnLoad><Frame/></OnLoad>"}) {
        std::string invalid="<Frame name='RejectedHandlerXML'><Scripts>"+std::string(body)+"</Scripts></Frame>";
        tree.reset(XMLTree_Load(invalid.c_str(),uint32_t(invalid.size())));
        Reject([&]{FrameXML_CreateFrame(XMLTree_GetRoot(tree.get()),nullptr,&status);}); tree.reset();
    }
    // A syntax failure reports status, leaves no stale registry ref and does not
    // poison the next valid owner operation.
    const char* invalid="<Frame><Scripts><OnLoad>local = broken</OnLoad></Scripts></Frame>";
    tree.reset(XMLTree_Load(invalid,std::strlen(invalid)));
    auto* failed=FrameXML_CreateFrame(XMLTree_GetRoot(tree.get()),nullptr,&status);
    Check(failed && !failed->m_onLoad.m_luaRef && !status.IsEmpty(),"XML handler syntax failure hidden or stale ref retained");
    delete failed; tree.reset(); status.Clear();
    RegionLua("NativeInvalidXMLFunction=42");
    const char* nonfunction="<Frame><Scripts><OnLoad function='NativeInvalidXMLFunction'/></Scripts></Frame>";
    tree.reset(XMLTree_Load(nonfunction,std::strlen(nonfunction)));
    failed=FrameXML_CreateFrame(XMLTree_GetRoot(tree.get()),nullptr,&status);
    Check(failed && !failed->m_onLoad.m_luaRef && !status.IsEmpty(),"XML global accepted a nonfunction handler");
    delete failed; tree.reset(); status.Clear();
    RegionLua("NativeInvalidXMLFunction=nil");
    delete xml1; delete xml2;
    Check(!FrameScript_GetEventListeners("NATIVE_HANDLER_EVENT")->Head(),"Destroyed XML listener remained subscribed");
    delete frame;
    RegionLua(R"(
        assert(NativeHandlerFrame==nil and NativeHandlerText==nil and HandlerXML1==nil and HandlerXML2==nil)
        HandlerLog,HandlerErrors,HandlerEvents,HandlerTaintSecure,NativeXMLUpdate,HandlerTaintCalled=nil,nil,nil,nil,nil,nil
        HandlerMetaCalls,HandlerOldGlobalMeta=nil,nil
        collectgarbage('collect')
    )");
    // Original seterrorhandler accepts functions only, while the default is
    // LUA_NOREF. Restore the fixture's separately retained reference exactly.
    if (s_errorHandlerFunc>0) luaL_unref(L,LUA_REGISTRYINDEX,s_errorHandlerFunc);
    s_errorHandlerFunc=previousErrorHandler;
    Check(lua_gettop(L)==top && lua_taintexpected==nesting,"Handlers left stack or taint nesting");
    Check(!Ink(Draw(device,root)),"Destroyed handler text retained pixels");
    std::cout << "frame-lua-handlers-hook-event-key-char-mouse-update-text-xml-cache-taint-error-teardown=passed\n";
}
