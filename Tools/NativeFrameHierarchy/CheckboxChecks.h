// Original CheckButton state/ownership/input, with optional unchanged Glue template.
#include "Engine/Source/Frame/CSimpleCheckbox.h"
static void CheckboxChecks(GlContext& context, LegacyDevice& device, CSimpleTop& root,
                           bool original, const std::string& snapshot) {
    const int frames=CSimpleFrame::s_frameCount;
    const int oldErrorHandler=s_errorHandlerFunc;
    s_errorHandlerFunc=0;
    RegionLua("seterrorhandler(function(err) return err end)");
    {
        std::vector<std::unique_ptr<CSimpleFrame>> pooled;
        for(unsigned i=0;i<51;++i) {
            pooled.emplace_back(Create_SimpleCheckButton(nullptr));
            Check(reinterpret_cast<uintptr_t>(pooled.back().get())%alignof(CSimpleCheckbox)==0,
                  "CheckButton pool lost native alignment");
        }
        Check(CSimpleCheckbox::s_allocator.m_dataUsed==51,"CheckButton factory bypassed pool");
    }
    Check(CSimpleFrame::s_frameCount==frames && !CSimpleCheckbox::s_allocator.m_dataUsed,
          "CheckButton pool retained slots");
    CStatus status;
    const char* document=R"(<Ui><CheckButton name="NativeCheckTemplate" virtual="true" checked="true"
      registerForClicks="LeftButtonUp,RightButtonUp">
      <Size x="200" y="80"/><Anchors><Anchor point="BOTTOMLEFT" x="64" y="64"/></Anchors>
      <NormalTexture name="$parentNormal" setAllPoints="true"><Color r="1" g="0" b="0"/></NormalTexture>
      <CheckedTexture name="$parentChecked" setAllPoints="true"><Color r="0" g="1" b="0"/></CheckedTexture>
      <DisabledCheckedTexture name="$parentDisabled" setAllPoints="true"><Color r="0" g="0" b="1"/></DisabledCheckedTexture>
      <Scripts><OnClick>CheckLog[#CheckLog+1]=self:GetChecked() and 'checked' or 'clear'</OnClick></Scripts>
    </CheckButton></Ui>)";
    std::unique_ptr<XMLTree,decltype(&XMLTree_Free)> tree(XMLTree_Load(document,std::strlen(document)),&XMLTree_Free);
    Check(bool(tree),"CheckButton fixture XML parse failed");
    FrameXML_StoreHashNode(XMLTree_GetRoot(tree.get())->m_child,"NativeCheckTemplate",nullptr,&status);tree.reset();
    RegionLua(R"(
        CheckLog={}
        local c=CreateFrame('CheckButton','NativeCheck',nil,'NativeCheckTemplate')
        assert(c:GetObjectType()=='CheckButton' and c:IsObjectType('Button') and c:IsObjectType('Frame'))
        assert(c:GetChecked()==1 and c:GetCheckedTexture()==NativeCheckChecked)
        assert(c:GetDisabledCheckedTexture()==NativeCheckDisabled)
        assert(not pcall(c.SetCheckedTexture,c,{[0]=NativeCheckChecked[0]}))
        assert(not pcall(c.SetDisabledCheckedTexture,c,NativeCheck))
        assert(not pcall(c.SetCheckedTexture,c,NativeCheckDisabled))
        assert(not pcall(c.SetNormalTexture,c,NativeCheckChecked))
        assert(not pcall(c.SetHighlightTexture,c,NativeCheckDisabled))
        assert(not pcall(c.SetDisabledCheckedTexture,c,NativeCheckNormal))
        c:SetCheckedTexture(NativeCheckChecked)
        c:SetChecked(false); assert(c:GetChecked()==nil)
        c:Click('RightButton'); assert(c:GetChecked()==1 and CheckLog[1]=='checked')
        c:SetScript('PreClick',function(self) CheckLog[#CheckLog+1]='pre' end)
        c:SetScript('OnClick',function(self,key)
            local value=self:GetChecked(); self:Click(); assert(self:GetChecked()==value)
            self:SetChecked(true); CheckLog[#CheckLog+1]=key
        end)
        c:SetScript('PostClick',function(self) CheckLog[#CheckLog+1]='post' end)
        c:SetChecked(false); CheckLog={}; StaleCheck=c
    )");
    auto* checkbox=static_cast<CSimpleCheckbox*>(CScriptObject::GetScriptObjectByName("NativeCheck",CSimpleCheckbox::GetObjectType()));
    Check(checkbox && checkbox->m_clickAction==(CLICK_ON_LEFTUP|CLICK_ON_RIGHTUP),"CheckButton click mask truncated");
    const auto normal=Draw(device,root);
    auto red=[](const CImVector& p){return p.r>240 && p.g<8 && p.b<8;};
    auto green=[](const CImVector& p){return p.g>240 && p.r<8 && p.b<8;};
    auto blue=[](const CImVector& p){return p.b>240 && p.r<8 && p.g<8;};
    Check(std::count_if(normal.begin(),normal.end(),red)>100,"Unchecked pixels missing");
    CRect hit;Check(checkbox->GetRect(&hit),"CheckButton rectangle unresolved");
    int width,height,pixelWidth,pixelHeight;context.Window().Size(width,height,pixelWidth,pixelHeight);
    const int x=int(std::lround(DDCToNDCWidth((hit.minX+hit.maxX)*.5f)*width));
    const int y=int(std::lround((1-DDCToNDCHeight((hit.minY+hit.maxY)*.5f))*height));
    auto deliver=[&](OSINPUT id,int key=0,int px=-1,int py=-1) {
        return Frame::Native::DispatchInputEvent(context,OSEVENT{id,{key,px<0?x:px,py<0?y:py,456}});
    };
    deliver(OS_INPUT_MOUSE_MOVE);deliver(OS_INPUT_MOUSE_DOWN,MOUSE_BUTTON_LEFT);
    Check(!checkbox->m_checked && root.m_mouseCapture==checkbox,"CheckButton toggled before release");
    deliver(OS_INPUT_MOUSE_UP,MOUSE_BUTTON_LEFT);
    RegionLua("assert(NativeCheck:GetChecked()==1 and table.concat(CheckLog,',')=='pre,LeftButton,post')");
    const auto checked=Draw(device,root);
    Check(std::count_if(checked.begin(),checked.end(),green)>100,"Checked pixels missing");
    Equal(checked,Draw(device,root));
    RegionLua("NativeCheck:Disable(); NativeCheck:Click(); assert(NativeCheck:GetChecked()==1)");
    const auto disabled=Draw(device,root);
    Check(std::count_if(disabled.begin(),disabled.end(),blue)>100,"Disabled checked pixels missing");
    deliver(OS_INPUT_MOUSE_DOWN,MOUSE_BUTTON_LEFT);deliver(OS_INPUT_MOUSE_UP,MOUSE_BUTTON_LEFT);
    Check(checkbox->m_checked,"Disabled mouse click toggled CheckButton");
    RegionLua("NativeCheck:Enable(); NativeCheck:SetScript('PreClick',nil); NativeCheck:SetScript('OnClick',nil); NativeCheck:SetScript('PostClick',nil); NativeCheck:SetChecked(false)");
    Equal(normal,Draw(device,root));
    deliver(OS_INPUT_MOUSE_DOWN,MOUSE_BUTTON_LEFT);
    deliver(OS_INPUT_MOUSE_MOVE,0,600,20);deliver(OS_INPUT_MOUSE_UP,MOUSE_BUTTON_LEFT,600,20);
    Check(!checkbox->m_checked && !root.m_mouseCapture,"Outside release toggled CheckButton");
    deliver(OS_INPUT_MOUSE_MOVE);deliver(OS_INPUT_MOUSE_DOWN,MOUSE_BUTTON_LEFT);
    deliver(OS_INPUT_FOCUS,0,0,0);
    Check(!checkbox->m_checked && !root.m_mouseCapture && checkbox->m_state==BUTTONSTATE_NORMAL,
          "Focus cancellation toggled or retained CheckButton");
    deliver(OS_INPUT_FOCUS,1,0,0);
    RegionLua("NativeCheck:SetChecked(true); NativeCheck:SetDisabledCheckedTexture(nil); NativeCheck:Disable()");
    Equal(checked,Draw(device,root)); // Original fallback to the enabled check texture.
    RegionLua("NativeCheck:Enable(); NativeCheck:SetCheckedTexture(nil); assert(NativeCheck:GetCheckedTexture()==nil)");
    Equal(normal,Draw(device,root));
    const char* file=original ? "Interface\\Buttons\\UI-CheckBox-Check" : "fixture.blp";
    Check(checkbox->SetCheckedTexture(file) && checkbox->SetDisabledCheckedTexture(file),
          "Filename check texture factory failed");
    Check(checkbox->m_checkedTexture->m_parent==checkbox && checkbox->m_checkedTexture->m_alphaMode==GxBlend_Add,
          "Filename check texture ownership/blend changed");
    {
        Frame::Native::CollectScope collect;
        Reject([&]{checkbox->SetChecked(0,0);});
        RegionLua("assert(not pcall(NativeCheck.SetChecked,NativeCheck,false))");
    }
    bool foreign=false;std::thread thread([&]{try{checkbox->SetChecked(0,0);}catch(const std::logic_error&){foreign=true;}});thread.join();
    Check(foreign,"Foreign CheckButton mutation accepted");
    delete checkbox;
    RegionLua("assert(not pcall(StaleCheck.GetChecked,StaleCheck)); StaleCheck=nil; CheckLog=nil");
    FrameXML_FreeHashNodes();
    Check(!Ink(Draw(device,root)),"Deleted CheckButton retained pixels");
    if(original) {
        uint8_t before[16],after[16];
        Check(FrameXML_CheckSignature("Interface\\GlueXML\\GlueXML.toc",nullptr,FrameXML::Native::OriginalInterfaceKey(),before)==3,
              "Original CheckButton pre-creation signature failed");
        tree.reset(FrameXML_LoadXML("Interface\\GlueXML\\GlueTemplates.xml",nullptr,&status));
        Check(bool(tree),"Original GlueTemplates read failed");
        XMLNode* selected=nullptr;
        for(auto* node=XMLTree_GetRoot(tree.get())->m_child;node;node=node->m_next)
            if(auto* name=node->GetAttributeByName("name");name && !std::strcmp(name,"GlueCheckButtonTemplate")) {selected=node;break;}
        Check(bool(selected),"Original GlueCheckButtonTemplate missing");
        FrameXML_StoreHashNode(selected,"GlueCheckButtonTemplate",nullptr,&status);tree.reset();
        RegionLua("CreateFrame('CheckButton','NativeOriginalCheck',nil,'GlueCheckButtonTemplate'); NativeOriginalCheckText:SetText('Remember account')");
        checkbox=static_cast<CSimpleCheckbox*>(CScriptObject::GetScriptObjectByName("NativeOriginalCheck",CSimpleCheckbox::GetObjectType()));
        Check(bool(checkbox),"Original CheckButton factory returned null");
        checkbox->SetPoint(FRAMEPOINT_CENTER,&root,FRAMEPOINT_CENTER,0,0,1);
        std::vector<CImVector> originalNormal;unsigned ready=0;
        for(;ready<4;++ready) {
            originalNormal=Draw(device,root);CRect rect;
            if(checkbox->m_activeTexture && checkbox->m_activeTexture->IsObjectLoaded() && checkbox->m_activeTexture->GetRect(&rect)) break;
        }
        Check(ready<4 && Ink(originalNormal)>50,"Original CheckButton normal pixels missing");
        checkbox->SetChecked(1,0);auto originalChecked=Draw(device,root);
        Check(Ink(originalChecked)>50 && std::memcmp(originalNormal.data(),originalChecked.data(),originalChecked.size()*sizeof(CImVector)),
              "Original CheckButton check did not render");
        Equal(originalChecked,Draw(device,root));
        checkbox->Enable(0);auto originalDisabled=Draw(device,root);
        Check(Ink(originalDisabled)>50 && std::memcmp(originalChecked.data(),originalDisabled.data(),originalChecked.size()*sizeof(CImVector)),
              "Original disabled CheckButton check did not render");
        checkbox->Enable(1);Equal(originalChecked,Draw(device,root));
        if(!snapshot.empty()) {
            std::ofstream out(snapshot+".checkbox.ppm",std::ios::binary);out<<"P6\n640 480\n255\n";
            for(int py=479;py>=0;--py) for(int px=0;px<640;++px) {
                const auto& p=originalChecked[py*640+px];char rgb[]={char(p.r),char(p.g),char(p.b)};out.write(rgb,3);
            }
            Check(bool(out),"Original CheckButton snapshot failed");
        }
        Check(FrameXML_CheckSignature("Interface\\GlueXML\\GlueXML.toc",nullptr,FrameXML::Native::OriginalInterfaceKey(),after)==3 && !std::memcmp(before,after,16),
              "Original CheckButton post-creation signature changed");
        delete checkbox;FrameXML_FreeHashNodes();
        std::cout<<"original-GlueCheckButtonTemplate-render-repeat-signature-subset=passed ready-frames="<<ready+1<<'\n';
    }
    if(!status.IsEmpty()) {char error[4096];throw std::runtime_error(status.GetErrorStr(error,sizeof(error),STATUS_WARNING,"; "));}
    Check(CSimpleFrame::s_frameCount==frames && !CSimpleCheckbox::s_allocator.m_dataUsed,"CheckButton retained objects/pool slots");
    luaL_unref(FrameScript_GetContext(),LUA_REGISTRYINDEX,s_errorHandlerFunc);s_errorHandlerFunc=oldErrorHandler;
    std::cout<<"frame-checkbox-pool-xml-click-state-texture-cancel=passed\n";
}
