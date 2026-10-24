// Original MessageFrame rows/private-font ownership, pending text and fading.
#include "Engine/Source/Frame/CSimpleMessageFrame.h"
static void MessageChecks(LegacyDevice& device, CSimpleTop& root, CSimpleFont& font,
                          const std::string& snapshot) {
    const int frames=CSimpleFrame::s_frameCount;
    const auto strings=CSimpleFontString::s_allocator.m_dataUsed;
    {
        std::vector<std::unique_ptr<CSimpleFrame>> pooled;
        for(unsigned i=0;i<7;++i) {
            pooled.emplace_back(Create_SimpleMessageFrame(nullptr));
            Check(uintptr_t(pooled.back().get())%alignof(CSimpleMessageFrame)==0,"MessageFrame pool alignment");
        }
        Check(CSimpleMessageFrame::s_allocator.m_dataUsed==7,"MessageFrame factory bypassed pool");
    }
    Check(!CSimpleMessageFrame::s_allocator.m_dataUsed,"MessageFrame pool retained slots");
    CStatus status;
    const char* document=R"(<Ui><MessageFrame name="NativeMessageTemplate" virtual="true" fade="false" insertMode="TOP"
       displayDuration="2" fadeDuration="1"><Size x="480" y="192"/>
       <Anchors><Anchor point="BOTTOMLEFT" x="64" y="64"/></Anchors>
       <TextInsets><AbsInset left="4" right="4" top="4" bottom="4"/></TextInsets>
       </MessageFrame></Ui>)";
    std::unique_ptr<XMLTree,decltype(&XMLTree_Free)> tree(XMLTree_Load(document,std::strlen(document)),&XMLTree_Free);
    Check(bool(tree),"MessageFrame XML parse");
    FrameXML_StoreHashNode(XMLTree_GetRoot(tree.get())->m_child,"NativeMessageTemplate",nullptr,&status);tree.reset();
    RegionLua(R"(
        local m=CreateFrame('MessageFrame','NativeMessages',nil,'NativeMessageTemplate')
        assert(m:GetObjectType()=='MessageFrame' and m:IsObjectType('Frame'))
        assert(m:GetInsertMode()=='TOP' and m:GetFading()==nil)
        assert(m:GetTimeVisible()==2 and m:GetFadeDuration()==1)
        assert(not pcall(m.SetFadeDuration,m,math.huge))
        assert(not pcall(m.SetTimeVisible,m,-1))
        StaleMessages=m
    )");
    auto* messages=static_cast<CSimpleMessageFrame*>(CScriptObject::GetScriptObjectByName("NativeMessages",CSimpleMessageFrame::GetObjectType()));
    Check(messages && !messages->m_rows,"Message rows allocated before font/layout readiness");
    messages->m_font->SetFontObject(&font);
    messages->m_font->m_attributes.SetJustifyH(1);
    messages->m_font->UpdateObjects();
    messages->AddMessage("A",CImVector(255,0,0,255));
    messages->AddMessage("AA",CImVector(0,255,0,255));
    Check(messages->m_pendingMessages.Count()==2,"Message text bypassed pending queue");
    auto first=Draw(device,root);
    Check(messages->m_rows>=2 && messages->m_numVisible==2 && !messages->m_pendingMessages.Count(),"Message rows did not consume pending text");
    Check(Ink(first)>10,"MessageFrame glyph pixels missing");Equal(first,Draw(device,root));
    Check(!std::strcmp(messages->m_lines[0].m_node->m_string->m_text,"AA") &&
          !std::strcmp(messages->m_lines[1].m_node->m_string->m_text,"A"),"Message insertion order");
    const auto rows=messages->m_rows;
    messages->SetHeight(messages->GetHeight()*1.5f);
    CLayoutFrame::ResizePending();
    Check(messages->m_nativeRowsDirty && messages->m_rows==rows,"Message resize changed ownership in callback");
    Draw(device,root);Check(messages->m_rows>rows,"Message rows did not grow after resize");
    messages->SetHeight(messages->GetHeight()/1.5f);Draw(device,root);
    RegionLua("NativeMessages:Clear(); NativeMessages:SetInsertMode('BOTTOM'); NativeMessages:AddMessage('A',1,1,1)");
    Draw(device,root);Check(messages->m_numVisible==1,"Bottom insertion visibility");
    const float firstOffset=messages->m_lines[0].m_offsetY;
    RegionLua("NativeMessages:SetInsertMode('TOP')");Draw(device,root);
    Check(messages->m_lines[0].m_offsetY<firstOffset,"Insert mode did not reposition rows");
    // Shrinking retires row text blocks before their private font; growing uses new nodes.
    messages->SetHeight(font.m_attributes.m_fontHeight*1.1f+messages->m_messageFrameInset.maxY+messages->m_messageFrameInset.minY);Draw(device,root);
    Check(messages->m_rows==1 && messages->m_lines.Count()==1,"Message row shrink ownership");
    messages->SetHeight(font.m_attributes.m_fontHeight*4.1f+messages->m_messageFrameInset.maxY+messages->m_messageFrameInset.minY);Draw(device,root);
    Check(messages->m_rows>=3,"Message row regeneration");
    RegionLua("NativeMessages:Clear(); NativeMessages:SetFading(true); NativeMessages:SetTimeVisible(.1); NativeMessages:SetFadeDuration(.2); NativeMessages:AddMessage('A')");
    const auto unfaded=Draw(device,root);
    root.OnLayerUpdate(.11f);root.OnLayerUpdate(.1f);
    auto faded=Draw(device,root);
    Check(messages->m_numVisible==1 && messages->m_lines[0].m_node->m_color.a<255,"Message fade did not update alpha");
    Check(std::memcmp(unfaded.data(),faded.data(),faded.size()*sizeof(CImVector)),"Message fading pixels unchanged");
    root.OnLayerUpdate(.3f);Check(!messages->m_numVisible && !Ink(Draw(device,root)),"Expired message retained pixels");
    // Repeated font updates and callbacks use the private subclass without losing its owner.
    RegionLua("NativeMessages:SetFading(false); NativeMessages:SetScript('OnUpdate',function(self) self:Clear() end)");
    Draw(device,root); // Callback admission stays bounded by the existing method guards.
    RegionLua("NativeMessages:SetScript('OnUpdate',nil)");
    messages->Clear();messages->AddMessage("A",CImVector(255,255,255,255));Draw(device,root);
    std::string excessive(4096,'A');Reject([&]{messages->AddMessage(excessive.c_str(),CImVector());});
    Reject([&]{messages->AddMessage("\xc0\x80",CImVector());});
    Reject([&]{messages->SetInsertMode(static_cast<SimpleMessageFrameInsertMode>(99));});
    {
        Frame::Native::CollectScope collect;
        Reject([&]{messages->AddMessage("A",CImVector());});
        RegionLua("assert(not pcall(NativeMessages.Clear,NativeMessages))");
    }
    bool foreign=false;std::thread thread([&]{try{messages->Clear();}catch(const std::logic_error&){foreign=true;}});thread.join();
    Check(foreign,"Foreign MessageFrame mutation accepted");
    messages->Clear();
    for(unsigned i=0;i<1024;++i)messages->AddMessage("A",CImVector(255,255,255,255));
    Reject([&]{messages->AddMessage("A",CImVector());});messages->Clear();
    // Distinct capture uses the selected authored/original font, with authored candidate-like text.
    messages->AddMessage("1: A",CImVector(255,255,255,255));
    messages->AddMessage("2: AA",CImVector(255,255,255,255));
    messages->AddMessage("> 1/2",CImVector(255,255,255,255));
    auto candidates=Draw(device,root);Equal(candidates,Draw(device,root));
    if(!snapshot.empty()) {
        std::ofstream out(snapshot+".messages.ppm",std::ios::binary);out<<"P6\n640 480\n255\n";
        for(int y=479;y>=0;--y)for(int x=0;x<640;++x){const auto& p=candidates[y*640+x];char rgb[]={char(p.r),char(p.g),char(p.b)};out.write(rgb,3);}
        Check(bool(out),"MessageFrame snapshot failed");
    }
    // Exhaust the native layout limit after some temporary rows exist. Failed
    // growth returns all temporary pooled strings and preserves existing owners.
    const float previousHeight=messages->GetHeight();
    const auto previousRows=messages->m_rows, previousStrings=CSimpleFontString::s_allocator.m_dataUsed;
    {
        std::vector<std::unique_ptr<CLayoutFrame>> pressure;
        for(unsigned i=0;i<900;++i)pressure.emplace_back(std::make_unique<CLayoutFrame>());
        messages->SetHeight(font.m_attributes.m_fontHeight*128.1f+messages->m_messageFrameInset.maxY+messages->m_messageFrameInset.minY);
        CLayoutFrame::ResizePending();
        Check(messages->m_nativeRowsDirty,"Pressure resize did not invalidate rows");
        Reject([&]{messages->PrepareRows();});
        Check(messages->m_rows==previousRows && messages->m_lines.Count()==previousRows &&
              CSimpleFontString::s_allocator.m_dataUsed==previousStrings && messages->m_nativeRowsDirty,
              "Failed MessageFrame growth leaked or replaced rows");
        messages->SetHeight(previousHeight);
    }
    Equal(candidates,Draw(device,root));
    delete messages;FrameXML_FreeHashNodes();
    RegionLua("assert(not pcall(StaleMessages.GetInsertMode,StaleMessages)); StaleMessages=nil");
    Check(CSimpleFrame::s_frameCount==frames && CSimpleFontString::s_allocator.m_dataUsed==strings &&
          !CSimpleMessageFrame::s_allocator.m_dataUsed && !Ink(Draw(device,root)),"MessageFrame teardown retained rows/font/pixels");
    std::cout<<"frame-message-private-font-pending-rows-resize-scroll-fade-lua-xml-owner-pool-teardown=passed\n";
}
