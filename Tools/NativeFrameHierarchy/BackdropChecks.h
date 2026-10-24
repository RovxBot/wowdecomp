// Authored repeat/pool/XML/Lua lifetimes, then unchanged original login backdrop.
#include "Engine/Source/Frame/CBackdropGenerator.h"
static Bytes BackdropEdgeBlp() {
    Bytes bytes(148);
    auto word=[&](size_t offset,uint32_t value){for(unsigned i=0;i<4;++i)bytes[offset+i]=uint8_t(value>>(i*8));};
    word(0,0x32504c42);word(4,1);bytes[8]=3;bytes[9]=8;word(12,128);word(16,16);word(20,148);word(84,128*16*4);
    for(unsigned y=0;y<16;++y)for(unsigned x=0;x<128;++x) {
        const uint8_t p[]={uint8_t(x/16*30),uint8_t(y<8?255:40),uint8_t(x%16<8?240:50),255};
        bytes.insert(bytes.end(),p,p+4);
    }
    return bytes;
}
static void BackdropChecks(LegacyDevice& device, CSimpleTop& root, bool original, const std::string& snapshot) {
    const int textures=CSimpleTexture::s_count;
    CStatus status;
    const char* document=R"(<Ui><Frame name="NativeBackdropTemplate" virtual="true">
      <Size x="256" y="128"/><Anchors><Anchor point="BOTTOMLEFT" x="64" y="64"/></Anchors>
      <Backdrop bgFile="backdrop.blp" edgeFile="backdrop-edge.blp" tile="true">
        <TileSize><AbsValue val="16"/></TileSize><EdgeSize val="16"/>
        <BackgroundInsets><AbsInset left="16" right="16" top="16" bottom="16"/></BackgroundInsets>
      </Backdrop></Frame></Ui>)";
    std::unique_ptr<XMLTree,decltype(&XMLTree_Free)> tree(XMLTree_Load(document,std::strlen(document)),&XMLTree_Free);
    Check(bool(tree),"Backdrop fixture XML parse failed");
    FrameXML_StoreHashNode(XMLTree_GetRoot(tree.get())->m_child,"NativeBackdropTemplate",nullptr,&status);tree.reset();
    RegionLua(R"(
        local f=CreateFrame('Frame','NativeBackdrop',nil,'NativeBackdropTemplate')
        local b=f:GetBackdrop(); assert(b.bgFile=='backdrop.blp' and b.tile==1)
        assert(math.abs(b.edgeSize-16)<0.001 and math.abs(b.tileSize-16)<0.001)
        assert(math.abs(b.insets.left-16)<0.001 and math.abs(b.insets.bottom-16)<0.001)
        local out=setmetatable({insets=setmetatable({}, {__newindex=function() error('unexpected inset hook') end})},
                                  {__newindex=function() error('unexpected field hook') end})
        assert(f:GetBackdrop(out)==out and out.bgFile==b.bgFile)
        assert(not pcall(f.SetBackdrop,f,{bgFile='backdrop.blp',edgeSize=0}))
        assert(not pcall(f.SetBackdrop,f,{bgFile='backdrop.blp',tileSize=0/0}))
        assert(not pcall(f.SetBackdrop,f,{bgFile='backdrop.blp',insets={left=math.huge}}))
        assert(f:GetBackdrop().bgFile==b.bgFile)
        b.insets={left=0,right=0,top=0,bottom=0}; f:SetBackdrop(b)
        StaleBackdrop=f
    )");
    auto* frame=static_cast<CSimpleFrame*>(CScriptObject::GetScriptObjectByName("NativeBackdrop",CSimpleFrame::GetObjectType()));
    Check(frame && frame->m_backdropGenerator && CSimpleTexture::s_count==textures+9,"Backdrop did not construct nine regions");
    auto* backdrop=frame->m_backdropGenerator;
    auto ready=[&] {
        for(unsigned count=0;count<4;++count) {
            auto pixels=Draw(device,root);
            bool loaded=true;
            for(auto* region=frame->m_regions.Head();region;region=frame->m_regions.Next(region))
                if(!region->IsObjectLoaded() || region->IsResizePending())loaded=false;
            if(loaded)return pixels;
        }
        throw std::runtime_error("Backdrop readiness exceeded four frames");
    };
    const float initialWidth=frame->GetWidth(),initialHeight=frame->GetHeight();
    auto baseline=ready();Check(Ink(baseline)>10000,"Backdrop pixels missing");Equal(baseline,Draw(device,root));
    for(auto* region=frame->m_regions.Head();region;region=frame->m_regions.Next(region))
        Check(uintptr_t(region)%alignof(CSimpleTexture)==0,"Backdrop region lost native alignment");
    Check(backdrop->m_backgroundTexture->m_texture->m_gxTexFlags.m_wrapU &&
          backdrop->m_backgroundTexture->m_texture->m_gxTexFlags.m_wrapV &&
          !TextureOnAtlas(backdrop->m_backgroundTexture->m_texture),"Backdrop repeat sampler/standalone storage changed");
    // The authored source has four colored quadrants. Compare corresponding
    // samples across complete periods in both axes, and require distinct phases.
    const auto& a=baseline[70*640+100];const auto& b=baseline[70*640+105];
    Check(std::memcmp(&a,&b,sizeof(a)),"Backdrop repeated source lost distinct phases");
    Check(!std::memcmp(&a,&baseline[70*640+110],sizeof(a)) &&
          !std::memcmp(&a,&baseline[80*640+100],sizeof(a)),"Backdrop did not repeat in both axes");
    frame->SetSize(initialWidth*1.25f,initialHeight*1.25f);
    auto resized=Draw(device,root);Check(Ink(resized)>Ink(baseline),"Resized backdrop did not regenerate");
    frame->SetSize(initialWidth,initialHeight);Equal(baseline,Draw(device,root));
    RegionLua("NativeBackdrop:SetBackdropColor(.2,.6,1,.5); NativeBackdrop:SetBackdropBorderColor(.4,1,.2,.5)");
    auto tinted=Draw(device,root);Check(std::memcmp(baseline.data(),tinted.data(),baseline.size()*sizeof(CImVector)),"Backdrop tint did not render");
    RegionLua("local r,g,b,a=NativeBackdrop:GetBackdropColor(); assert(math.abs(a-.5)<.01 and g>.59 and g<.61); NativeBackdrop:SetBackdropColor(1,1,1,1); NativeBackdrop:SetBackdropBorderColor(1,1,1,1)");
    Equal(baseline,Draw(device,root));
    // Fail after the new background region has been created, using an invalid
    // edge filename. All temporary slots return; previous output remains live.
    auto failed=std::make_unique<CBackdropGenerator>();failed->m_bgFile.Copy("backdrop.blp");
    failed->m_edgeFile.Copy("../invalid.blp");failed->m_pieces=CBackdropGenerator::THEWORKS;
    bool threw=false;try{frame->SetBackdrop(failed.get());}catch(const std::exception&){threw=true;}
    Check(threw && frame->m_backdropGenerator==backdrop && CSimpleTexture::s_count==textures+9,
          "Failed backdrop replacement retained regions or lost prior output");
    Equal(baseline,Draw(device,root));
    {
        Frame::Native::CollectScope collect;
        Reject([&]{frame->SetBackdrop(nullptr);});Reject([&]{backdrop->SetVertexColor(CImVector(1,2,3,4));});
        RegionLua("assert(not pcall(NativeBackdrop.SetBackdrop,NativeBackdrop,nil))");
    }
    bool foreign=false;std::thread thread([&]{try{frame->SetBackdrop(nullptr);}catch(const std::logic_error&){foreign=true;}});thread.join();
    Check(foreign,"Foreign backdrop replacement accepted");
    RegionLua("local b=NativeBackdrop:GetBackdrop(); NativeBackdrop:SetBackdrop(nil); assert(NativeBackdrop:GetBackdrop()==nil); NativeBackdrop:SetBackdrop(b)");
    baseline=ready(); // Lua retains original dimensions and template settings.
    Check(Ink(baseline)>10000,"Recreated backdrop lost pixels");
    delete frame;FrameXML_FreeHashNodes();
    RegionLua("assert(not pcall(StaleBackdrop.GetBackdrop,StaleBackdrop)); StaleBackdrop=nil");
    Check(CSimpleTexture::s_count==textures && !Ink(Draw(device,root)),"Deleted backdrop retained regions/pixels");
    if(original) {
        uint8_t before[16],after[16];
        Check(FrameXML_CheckSignature("Interface\\GlueXML\\GlueXML.toc",nullptr,FrameXML::Native::OriginalInterfaceKey(),before)==3,
              "Original backdrop pre-creation signature failed");
        tree.reset(FrameXML_LoadXML("Interface\\GlueXML\\AccountLogin.xml",nullptr,&status));
        Check(bool(tree),"Original AccountLogin XML read failed");
        XMLNode* selected=nullptr;XMLNode* edit=nullptr;
        std::function<void(XMLNode*)> find=[&](XMLNode* node){
            for(;node;node=node->m_next) {
                if(!SStrCmpI(node->m_name.m_str,"EditBox",STORM_MAX_STR) && node->GetChildByName("Backdrop") && !selected) {
                    edit=node;selected=node->GetChildByName("Backdrop");
                }
                find(node->m_child);
            }
        };
        find(XMLTree_GetRoot(tree.get()));Check(selected && edit,"Original login EditBox backdrop missing");
        auto originalBackdrop=std::make_unique<CBackdropGenerator>();originalBackdrop->LoadXML(selected,&status);
        auto originalFrame=std::unique_ptr<CSimpleFrame>(Create_SimpleFrame(nullptr));frame=originalFrame.get();
        float width=0,height=0;Check(LoadXML_Dimensions(edit->GetChildByName("Size"),width,height,&status),"Original field size missing");
        frame->SetSize(width,height);frame->SetPoint(FRAMEPOINT_CENTER,&root,FRAMEPOINT_CENTER,0,0,1);
        frame->SetBackdrop(originalBackdrop.get());originalBackdrop.release();
        tree.reset(); // All image paths/settings must survive source-tree close.
        auto pixels=ready();Check(Ink(pixels)>100,"Original login backdrop pixels missing");Equal(pixels,Draw(device,root));
        Check(frame->m_backdropGenerator->m_tile==1,"Original login background is no longer tiled");
        if(!snapshot.empty()) {
            std::ofstream out(snapshot+".backdrop.ppm",std::ios::binary);out<<"P6\n640 480\n255\n";
            for(int y=479;y>=0;--y)for(int x=0;x<640;++x) {
                const auto& p=pixels[y*640+x];char rgb[]={char(p.r),char(p.g),char(p.b)};out.write(rgb,3);
            }
            Check(bool(out),"Original backdrop snapshot failed");
        }
        Check(FrameXML_CheckSignature("Interface\\GlueXML\\GlueXML.toc",nullptr,FrameXML::Native::OriginalInterfaceKey(),after)==3 && !std::memcmp(before,after,16),
              "Original backdrop post-creation subset signature changed");
        std::cout<<"original-AccountLogin-backdrop-render-repeat-signature-subset=passed\n";
    }
    if(!status.IsEmpty()){char error[4096];throw std::runtime_error(status.GetErrorStr(error,sizeof(error),STATUS_WARNING,"; "));}
    Check(CSimpleTexture::s_count==textures,"Backdrop teardown retained regions");
    std::cout<<"frame-backdrop-xml-lua-repeat-replace-resize-owner-pool-teardown=passed\n";
}
