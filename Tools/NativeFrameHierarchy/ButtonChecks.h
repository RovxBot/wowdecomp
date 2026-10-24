// Original Button consumers with authored XML/input; optional unchanged Glue template.
#include "Engine/Source/Frame/CSimpleButton.h"
static void ButtonChecks(GlContext& context, LegacyDevice& device, CSimpleTop& root, CSimpleFont& font, bool original, const std::string& snapshot) {
    const int frames = CSimpleFrame::s_frameCount;
    const int oldErrorHandler=s_errorHandlerFunc;
    // Keep fixture errors observable even when the product default handler is nil.
    // Retain the old registry entry until this fixture restores it.
    s_errorHandlerFunc=0;
    RegionLua("seterrorhandler(function(err) return err end)");
    {
        std::vector<std::unique_ptr<CSimpleFrame>> pooled;
        for (unsigned i=0;i<101;++i) {
            pooled.emplace_back(Create_SimpleButton(nullptr));
            Check(reinterpret_cast<uintptr_t>(pooled.back().get()) % alignof(CSimpleButton)==0,
                  "Button pool lost native alignment");
        }
        Check(CSimpleButton::s_allocator.m_dataUsed==101,"Button factory bypassed original pool");
    }
    Check(CSimpleFrame::s_frameCount==frames && !CSimpleButton::s_allocator.m_dataUsed,"Button pool did not release slots");
    CStatus status;
    const char* document=R"(<Ui><Button name="NativeButtonTemplate" virtual="true" registerForClicks="LeftButtonUp,RightButtonUp">
      <Size x="200" y="80"/><Anchors><Anchor point="BOTTOMLEFT" x="64" y="64"/></Anchors>
      <NormalTexture name="$parentNormal" setAllPoints="true"><Color r="1" g="0" b="0"/></NormalTexture>
      <PushedTexture name="$parentPushed" setAllPoints="true"><Color r="0" g="1" b="0"/></PushedTexture>
      <DisabledTexture name="$parentDisabled" setAllPoints="true"><Color r="0" g="0" b="1"/></DisabledTexture>
      <HighlightTexture name="$parentHighlight" setAllPoints="true" alphaMode="ADD"><Color r="0" g="0" b=".25"/></HighlightTexture>
      <ButtonText name="$parentText" text="A"/>
      <NormalFont/><DisabledFont/><HighlightFont/><PushedTextOffset x="2" y="-2"/>
      <Scripts><OnClick>ButtonLog[#ButtonLog+1]='xml:'..button</OnClick></Scripts>
    </Button></Ui>)";
    std::unique_ptr<XMLTree,decltype(&XMLTree_Free)> tree(XMLTree_Load(document,std::strlen(document)),&XMLTree_Free);
    Check(bool(tree),"Button fixture XML parse failed");
    auto* node=XMLTree_GetRoot(tree.get())->m_child;
    for (auto* name : {"NormalFont","DisabledFont","HighlightFont"}) node->GetChildByName(name)->SetAttribute("style",font.GetName());
    FrameXML_StoreHashNode(node,"NativeButtonTemplate",nullptr,&status);tree.reset();
    if (font.m_fontObject) lua_rawgeti(FrameScript_GetContext(),LUA_REGISTRYINDEX,font.m_fontObject->GetLuaObjectRef());
    else lua_pushnil(FrameScript_GetContext());
    lua_setglobal(FrameScript_GetContext(),"ExpectedButtonFontParent");
    RegionLua(R"(
        ButtonLog={}
        local b=CreateFrame('Button','NativeButton',nil,'NativeButtonTemplate')
        assert(b:GetObjectType()=='Button' and b:IsObjectType('Frame'))
        assert(b:IsEnabled()==1 and b:GetButtonState()=='NORMAL')
        assert(b:GetNormalTexture()==NativeButtonNormal and b:GetPushedTexture()==NativeButtonPushed)
        assert(b:GetDisabledTexture()==NativeButtonDisabled and b:GetHighlightTexture()==NativeButtonHighlight)
        assert(b:GetFontString()==NativeButtonText and b:GetNormalFontObject()==ExpectedButtonFontParent)
        assert(b:HasScript('PreClick') and b:HasScript('OnClick') and b:HasScript('PostClick') and b:HasScript('OnDoubleClick'))
        assert(not pcall(b.SetNormalTexture,b,{[0]=NativeButtonNormal[0]}))
        assert(not pcall(b.SetNormalTexture,b,NativeButtonPushed))
        assert(not pcall(b.SetHighlightTexture,b,NativeButtonNormal))
        assert(not pcall(b.SetFontString,b,{[0]=NativeButtonText[0]}))
        assert(not pcall(b.SetNormalFontObject,b,{[0]=NativeButtonNormal[0]}))
        assert(not pcall(b.SetPushedTextOffset,b,0/0,1))
        b:SetPushedTextOffset(2,-2)
        local x,y=b:GetPushedTextOffset(); assert(math.abs(x-2)<.001 and math.abs(y+2)<.001)
        b:Click('RightButton'); assert(ButtonLog[1]=='xml:RightButton')
        ButtonLog={}
        b:SetScript('PreClick',function(self,key,down) ButtonLog[#ButtonLog+1]='pre:'..key end)
        b:SetScript('OnClick',function(self,key,down)
            assert(self==NativeButton and self==this and arg1==key and not down)
            assert(not pcall(CreateFrame,'Frame','RejectedButtonCallback'))
            assert(not pcall(self.Hide,self))
            self:SetText('V'); self:Click(key) -- reentry must not click again
            ButtonLog[#ButtonLog+1]='click:'..key
        end)
        b:SetScript('PostClick',function(self,key,down) ButtonLog[#ButtonLog+1]='post:'..key end)
        b:SetScript('OnEnter',function(self,motion) ButtonEnters=(ButtonEnters or 0)+1 end)
        b:SetScript('OnLeave',function(self,motion) ButtonLeaves=(ButtonLeaves or 0)+1 end)
        StaleButton=b
    )");
    auto* button=static_cast<CSimpleButton*>(CScriptObject::GetScriptObjectByName("NativeButton",CSimpleButton::GetObjectType()));
    Check(button && button->m_normalFont==&font && button->m_clickAction==(CLICK_ON_LEFTUP|CLICK_ON_RIGHTUP),"Button XML lost high click-mask bits");
    CLayoutFrame::ResizePending(); CRect hit;
    Check(button->GetRect(&hit),"Button hit rectangle is unresolved");
    int width,height,pixelWidth,pixelHeight; context.Window().Size(width,height,pixelWidth,pixelHeight);
    const int insideX=int(std::lround(DDCToNDCWidth((hit.minX+hit.maxX)*.5f)*width));
    const int insideY=int(std::lround((1-DDCToNDCHeight((hit.minY+hit.maxY)*.5f))*height));
    auto deliver=[&](OSINPUT id,int button=0,int x=-1,int y=-1) {
        return Frame::Native::DispatchInputEvent(context,OSEVENT{id,{button,x<0?insideX:x,y<0?insideY:y,456}});
    };
    auto red=[](const CImVector& p){return p.r>240 && p.g<8 && p.b<8;};
    auto green=[](const CImVector& p){return p.g>240 && p.r<8 && p.b<100;};
    auto blue=[](const CImVector& p){return p.b>240 && p.r<8 && p.g<8;};
    deliver(OS_INPUT_MOUSE_MOVE,0,600,20);
    const auto normal=Draw(device,root);Check(std::count_if(normal.begin(),normal.end(),red)>100,"Button normal texture missing");
    Equal(normal,Draw(device,root));
    deliver(OS_INPUT_MOUSE_MOVE);
    RegionLua("assert(ButtonEnters==1)");
    const auto highlighted=Draw(device,root);
    Check(std::memcmp(normal.data(),highlighted.data(),normal.size()*sizeof(CImVector)),"Button hover highlight did not render");
    Check(deliver(OS_INPUT_MOUSE_DOWN,MOUSE_BUTTON_LEFT)==InputResult::Handled && root.m_mouseCapture==button && button->m_state==BUTTONSTATE_PUSHED,
          "Button down/capture did not select pushed state");
    const auto pushed=Draw(device,root);Check(std::count_if(pushed.begin(),pushed.end(),green)>100,"Button pushed texture missing");
    deliver(OS_INPUT_MOUSE_UP,MOUSE_BUTTON_LEFT);
    RegionLua("assert(table.concat(ButtonLog,',')=='pre:LeftButton,click:LeftButton,post:LeftButton'); assert(NativeButton:GetText()=='V'); ButtonLog={}");
    Check(button->m_state==BUTTONSTATE_NORMAL && !root.m_mouseCapture && !button->m_inClick && !root.m_mouseButtonName,"Button release/callback state not restored");
    // Right-up lives above the original 32-bit XML accumulator's range.
    deliver(OS_INPUT_MOUSE_DOWN,MOUSE_BUTTON_RIGHT);deliver(OS_INPUT_MOUSE_UP,MOUSE_BUTTON_RIGHT);
    RegionLua("assert(table.concat(ButtonLog,',')=='pre:RightButton,click:RightButton,post:RightButton'); ButtonLog={}");
    deliver(OS_INPUT_MOUSE_DOWN,MOUSE_BUTTON_LEFT);
    deliver(OS_INPUT_MOUSE_MOVE,0,600,20);deliver(OS_INPUT_MOUSE_UP,MOUSE_BUTTON_LEFT,600,20);
    RegionLua("assert(#ButtonLog==0)");Check(button->m_state==BUTTONSTATE_NORMAL,"Button outside release stayed pushed");
    RegionLua("NativeButton:Disable(); assert(NativeButton:IsEnabled()==0 and NativeButton:GetButtonState()=='DISABLED')");
    const auto disabled=Draw(device,root);Check(std::count_if(disabled.begin(),disabled.end(),blue)>100,"Button disabled texture missing");
    deliver(OS_INPUT_MOUSE_MOVE);deliver(OS_INPUT_MOUSE_DOWN,MOUSE_BUTTON_LEFT);deliver(OS_INPUT_MOUSE_UP,MOUSE_BUTTON_LEFT);
    RegionLua("NativeButton:Click(); assert(#ButtonLog==0); NativeButton:Enable(); NativeButton:SetButtonState('PUSHED',true)");
    deliver(OS_INPUT_MOUSE_DOWN,MOUSE_BUTTON_LEFT);deliver(OS_INPUT_MOUSE_UP,MOUSE_BUTTON_LEFT);
    Check(button->m_state==BUTTONSTATE_PUSHED && button->m_stateLocked,"Locked Button state changed on release");
    RegionLua("NativeButton:SetButtonState('NORMAL',false); NativeButton:SetScript('OnClick',nil); NativeButton:SetScript('PreClick',nil); NativeButton:SetScript('PostClick',nil)");
    RegionLua("NativeButton:SetScript('OnDoubleClick',function(self,key) ButtonDoubles=(ButtonDoubles or 0)+1 end)");
    button->m_lastClickTime=OsGetAsyncTimeMs();
    deliver(OS_INPUT_MOUSE_DOWN,MOUSE_BUTTON_LEFT);deliver(OS_INPUT_MOUSE_UP,MOUSE_BUTTON_LEFT);
    RegionLua("assert(ButtonDoubles==1)");
    deliver(OS_INPUT_MOUSE_DOWN,MOUSE_BUTTON_LEFT);
    deliver(OS_INPUT_FOCUS,0,0,0);
    Check(!root.m_mouseCapture && button->m_state==BUTTONSTATE_NORMAL,"Button focus cancellation stayed pushed");
    {
        Frame::Native::CollectScope collection;
        RegionLua("assert(not pcall(NativeButton.Click,NativeButton)); assert(not pcall(NativeButton.SetButtonState,NativeButton,'PUSHED'))");
    }
    CSimpleButtonState invalid;uint32_t raw=3;std::memcpy(&invalid,&raw,sizeof(raw));
    Reject([&]{button->SetButtonState(invalid,0);});
    Reject([&]{button->SetStateTexture(invalid,static_cast<CSimpleTexture*>(nullptr));});
    bool foreign=false;std::thread thread([&]{try{button->OnClick("LeftButton",0);}catch(const std::logic_error&){foreign=true;}});thread.join();
    Check(foreign,"Foreign Button click accepted");
    root.CancelMouseInput();delete button;
    RegionLua("assert(not pcall(StaleButton.Click,StaleButton)); StaleButton=nil; ButtonLog=nil; ButtonDoubles=nil; ButtonEnters=nil; ButtonLeaves=nil; ExpectedButtonFontParent=nil");
    FrameXML_FreeHashNodes();
    struct ThrowButton final : CSimpleButton {
        ThrowButton() : CSimpleButton(nullptr) {}
        void OnClick(const char*, int32_t) override { throw std::runtime_error("Authored Button click failure"); }
    };
    {
        auto failing=std::make_unique<ThrowButton>();
        failing->SetClickAction(CLICK_ON_LEFTDOWN);
        CMouseEvent click;click.m_data.m_button=MOUSE_BUTTON_LEFT;
        root.unused_1250=7;
        bool failed=false;
        try { failing->OnLayerMouseDown(click,"LeftButton"); } catch(const std::runtime_error&) { failed=true; }
        Check(failed && root.unused_1250==7,"Failed Button click leaked root dispatch state");
        root.unused_1250=0;
    }
    Check(!Ink(Draw(device,root)),"Deleted Button left retained pixels");
    if (original) {
        uint8_t before[16],after[16];
        Check(FrameXML_CheckSignature("Interface\\GlueXML\\GlueXML.toc",nullptr,FrameXML::Native::OriginalInterfaceKey(),before)==3,"Original Button pre-creation signature failed");
        tree.reset(FrameXML_LoadXML("Interface\\GlueXML\\GlueTemplates.xml",nullptr,&status));
        Check(bool(tree),"Original GlueTemplates read failed");
        for (auto* item=XMLTree_GetRoot(tree.get())->m_child;item;) {
            auto* next=item->m_next;
            if (const char* name=item->GetAttributeByName("name");name && item->GetAttributeByName("virtual"))
                FrameXML_StoreHashNode(item,name,nullptr,&status);
            item=next;
        }
        tree.reset();
        XMLNode instance(nullptr,"Button");
        instance.SetAttribute("name","NativeOriginalButton"); instance.SetAttribute("inherits","GlueButtonTemplate");
        auto* originalButton=static_cast<CSimpleButton*>(FrameXML_CreateFrame(&instance,nullptr,&status));
        Check(bool(originalButton),"Original Button factory returned null");
        originalButton->SetText("AV login"); originalButton->SetSize(NDCToDDCWidth(200.0f/640),NDCToDDCHeight(40.0f/480));
        originalButton->SetPoint(FRAMEPOINT_CENTER,&root,FRAMEPOINT_CENTER,0,0,1);
        auto originalPixels=Draw(device,root);
        Check(Ink(originalPixels)>100,"Unchanged original GlueButtonTemplate did not render");
        Equal(originalPixels,Draw(device,root));
        if (!snapshot.empty()) {
            std::ofstream out(snapshot+".button.ppm",std::ios::binary); out<<"P6\n640 480\n255\n";
            for (int y=479;y>=0;--y) for(int x=0;x<640;++x) {
                const auto& p=originalPixels[y*640+x]; char rgb[]={char(p.r),char(p.g),char(p.b)}; out.write(rgb,3);
            }
            Check(bool(out),"Original Button snapshot failed");
        }
        Check(FrameXML_CheckSignature("Interface\\GlueXML\\GlueXML.toc",nullptr,FrameXML::Native::OriginalInterfaceKey(),after)==3 && !std::memcmp(before,after,16),"Original Button post-creation signature changed");
        delete static_cast<CSimpleFrame*>(CScriptObject::GetScriptObjectByName("NativeOriginalButton",CSimpleFrame::GetObjectType()));
        FrameXML_FreeHashNodes();
        std::cout<<"original-GlueButtonTemplate-render-repeat-signature-subset=passed\n";
    }
    Check(CSimpleFrame::s_frameCount==frames && !CSimpleButton::s_allocator.m_dataUsed,"Button fixture retained objects/pool slots");
    luaL_unref(FrameScript_GetContext(),LUA_REGISTRYINDEX,s_errorHandlerFunc);
    s_errorHandlerFunc=oldErrorHandler;
    std::cout<<"frame-button-pool-xml-click-state-texture-font-callback-cancel=passed\n";
}
