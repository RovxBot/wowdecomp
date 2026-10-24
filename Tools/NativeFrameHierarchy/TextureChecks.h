// Authored texture region/XML/Lua coverage; optional original resource bytes.
static Bytes TextureFixtureBlp() {
    Bytes bytes(148);
    auto word = [&](size_t offset, uint32_t value) {
        for (unsigned i=0;i<4;++i) bytes[offset+i] = uint8_t(value >> (i*8));
    };
    word(0,0x32504c42);word(4,1);bytes[8]=3;bytes[9]=8;bytes[11]=0x10;
    word(12,8);word(16,8);word(20,148);word(84,256);
    for (unsigned y=0;y<8;++y) for(unsigned x=0;x<8;++x) {
        const uint8_t pixel[]{uint8_t(y<4?0:255),uint8_t(x<4?0:255),uint8_t(x<4?255:0),255};
        bytes.insert(bytes.end(),pixel,pixel+4);
    }
    return bytes;
}
// Authored 32x8 DXT5 with original area-sized thin mip tails. Frame's explicit
// linear sampler consumes only the complete base level; no tail pixels exist.
static Bytes ThinTextureFixtureBlp() {
    Bytes bytes(148);
    auto word=[&](size_t offset,uint32_t value){for(unsigned i=0;i<4;++i)bytes[offset+i]=uint8_t(value>>(i*8));};
    word(0,0x32504c42);word(4,1);bytes[8]=2;bytes[9]=8;bytes[10]=7;bytes[11]=1;word(12,32);word(16,8);
    const uint8_t block[16]={255,0,0,0,0,0,0,0,0,0xf8,0xe0,7,0,0,0,0};
    for(unsigned level=0,w=32,h=8;level<6;++level,w=std::max(1u,w/2),h=std::max(1u,h/2)) {
        unsigned count=std::max(1u,w*h/16);word(20+level*4,uint32_t(bytes.size()));word(84+level*4,count*16);
        for(unsigned b=0;b<count;++b)bytes.insert(bytes.end(),block,block+16);
    }
    return bytes;
}
static void TextureChecks(LegacyDevice& device, CSimpleTop& root, bool original) {
    const int before = CSimpleTexture::s_count;
    {
        std::vector<std::unique_ptr<CSimpleTexture>> pooled;
        for (unsigned i=0;i<737;++i) {
            pooled.emplace_back(Frame::Native::CreateTexture(nullptr,2,false));
            Check(reinterpret_cast<uintptr_t>(pooled.back().get()) % alignof(CSimpleTexture)==0,
                  "Texture pool lost native alignment");
        }
        Check(CSimpleTexture::s_count==before+737 && CSimpleTexture::s_allocator.m_dataUsed==737,
              "Texture pool growth bypassed original allocator");
    }
    Check(CSimpleTexture::s_count==before && !CSimpleTexture::s_allocator.m_dataUsed,
          "Texture pool growth teardown retained slots");
    auto* parent = Create_SimpleFrame(nullptr);
    parent->SetName("NativeTextureParent");
    parent->SetSize(.5f,.4f);
    parent->SetPoint(FRAMEPOINT_BOTTOMLEFT,&root,FRAMEPOINT_BOTTOMLEFT,.1f,.1f,1);
    const char* virtualXML=R"(<Ui><Texture name="NativeTextureTemplate" hidden="true" virtual="true">
      <Color r="0" g="1" b="0" a="1"/></Texture></Ui>)";
    {
        std::unique_ptr<XMLTree,decltype(&XMLTree_Free)> templates(XMLTree_Load(virtualXML,std::strlen(virtualXML)),&XMLTree_Free);
        Check(bool(templates),"Texture template parse failed");
        CStatus status;
        auto* node=XMLTree_GetRoot(templates.get())->m_child;
        FrameXML_StoreHashNode(node,"NativeTextureTemplate",nullptr,&status);
    }
    RegionLua(R"(
        local inherited=NativeTextureParent:CreateTexture('$parentInherited','BACKGROUND','NativeTextureTemplate')
        assert(inherited:GetParent()==NativeTextureParent and not inherited:IsShown())
        assert(inherited:GetTexture()=='SolidTexture')
        assert(not pcall(NativeTextureParent.CreateTexture,NativeTextureParent,'RejectedTextureList','BACKGROUND','NativeTextureTemplate,NativeTextureTemplate'))
        assert(RejectedTextureList==nil)
        local t = NativeTextureParent:CreateTexture('$parentImage','ARTWORK')
        assert(t == NativeTextureParentImage and t:GetParent() == NativeTextureParent)
        assert(t:IsObjectType('Texture') and t:GetDrawLayer() == 'ARTWORK')
        t:SetAllPoints(NativeTextureParent)
        assert(t:SetTexture(1,0,0,1))
        assert(t:GetTexture() == 'SolidTexture')
        assert(not pcall(t.SetTexCoord,t,0/0,1,0,1))
        assert(not pcall(t.SetTexture,t,0/0,0,0))
        assert(not pcall(NativeTextureParent.CreateTexture,NativeTextureParent,'$parentImage'))
        assert(not pcall(NativeTextureParent.CreateTexture,NativeTextureParent,'RejectedTexture','OVERLAY','AbsentTexture'))
        assert(RejectedTexture == nil)
        StaleTexture = t
    )");
    auto* texture = static_cast<CSimpleTexture*>(CScriptObject::GetScriptObjectByName("NativeTextureParentImage",CSimpleTexture::GetObjectType()));
    Check(texture && !texture->m_textureFlags && texture->m_texture && texture->m_texture->m_gxWidth==8,
          "Native pooled texture initialization/solid extent");
    auto solid=Draw(device,root);
    Check(Ink(solid)>100,"Solid texture region did not render");
    Check(std::any_of(solid.begin(),solid.end(),[](const CImVector& p){return p.r>240 && p.g<8 && p.b<8;}),
          "Solid texture BGR convention changed");
    Equal(solid,Draw(device,root));
    // Change only this non-tiled region's layout, leaving its parent and the
    // resource untouched. A retained vertex snapshot must follow both moves.
    texture->MarkFramePointsUnused();
    texture->SetPoint(FRAMEPOINT_TOPLEFT,parent,FRAMEPOINT_TOPLEFT,.1f,0,1);
    texture->SetPoint(FRAMEPOINT_BOTTOMRIGHT,parent,FRAMEPOINT_BOTTOMRIGHT,.1f,0,1);
    const auto moved=Draw(device,root);
    Check(Ink(moved)==Ink(solid) && std::memcmp(moved.data(),solid.data(),solid.size()*sizeof(CImVector)),
          "Non-tiled texture layout retained stale batch vertices");
    texture->SetAllPoints(parent,1);
    Equal(solid,Draw(device,root));
    RegionLua("assert(NativeTextureParentImage:SetDesaturated(true)); assert(NativeTextureParentImage:IsDesaturated())");
    auto gray=Draw(device,root);
    Check(std::any_of(gray.begin(),gray.end(),[](const CImVector& p){return p.r>20 && std::abs(int(p.r)-p.g)<=2 && std::abs(int(p.r)-p.b)<=2;}),
          "Texture desaturation shader did not execute");
    RegionLua("NativeTextureParentImage:SetDesaturated(false)");
    Equal(solid,Draw(device,root));
    if (!original) {
        Check(texture->SetTexture("thin.blp",0,0,GxTex_Linear,ImageMode_Normal),"Thin-tail UI texture request failed");
        auto base=Draw(device,root);
        Check(std::count_if(base.begin(),base.end(),[](const CImVector& p){return p.r>240 && p.g<8 && p.b<8;})>100,
              "Base-only image Services lost complete DXT pixels while validating unrequested thin tails");
    }
    const char* filename=original ? "Interface\\Glues\\LoadingBar\\Loading-BarFill.blp" : "fixture.blp";
    TextureEnableAtlas(1);
    Check(texture->SetTexture(filename,0,0,GxTex_Nearest,ImageMode_Normal),"File texture replacement failed");
    auto pixels=Draw(device,root);
    Check(bool(TextureOnAtlas(texture->m_texture))==!original,"Frame texture atlas eligibility changed");
    TextureEnableAtlas(0);
    Check(Ink(pixels)>100,"File-backed texture region did not render");
    Equal(pixels,Draw(device,root));
    RegionLua("NativeTextureParentImage:SetTexCoord(.1,.5,.1,.9)");
    auto cropped=Draw(device,root);
    Check(std::memcmp(pixels.data(),cropped.data(),pixels.size()*sizeof(CImVector)),"Texture coordinate mutation retained stale batch pixels");
    RegionLua("NativeTextureParentImage:SetTexCoord(0,1,0,1)");
    Equal(pixels,Draw(device,root));
    RegionLua("NativeTextureParentImage:Hide()");
    Check(!Ink(Draw(device,root)),"Hidden texture region remained batched");
    RegionLua("NativeTextureParentImage:Show(); NativeTextureParentImage:SetAlpha(.5)");
    auto faded=Draw(device,root);
    Check(Ink(faded)>0 && std::memcmp(pixels.data(),faded.data(),pixels.size()*sizeof(CImVector)),"Texture alpha mutation did not render");
    RegionLua("NativeTextureParentImage:SetAlpha(1)");
    Equal(pixels,Draw(device,root));
    {
        Frame::Native::CollectScope traversal;
        RegionLua("assert(not pcall(NativeTextureParent.CreateTexture,NativeTextureParent,'RejectedTraversalTexture')); assert(RejectedTraversalTexture == nil)");
    }
    Reject([&]{Frame::Native::CreateTexture(parent,5,true);});
    // Retained quad handles survive region destruction until the next dirty rebuild.
    delete texture;
    Check(!Ink(Draw(device,root)),"Deleted texture region retained visible geometry");
    const char* document=R"(<Frame name="NativeTextureXML" parent="NativeTextureParent">
      <Size x="180" y="90"/><Anchors><Anchor point="BOTTOMLEFT"/></Anchors>
      <Layers><Layer level="OVERLAY"><Texture name="$parentFill" parentKey="Fill" setAllPoints="true">
        <Color r="0" g="1" b="0" a="1"/>
        <TexCoords left="0" right="1" top="0" bottom="1"/>
      </Texture></Layer></Layers></Frame>)";
    std::unique_ptr<XMLTree,decltype(&XMLTree_Free)> tree(XMLTree_Load(document,std::strlen(document)),&XMLTree_Free);
    Check(bool(tree),"Texture XML parse failed");
    CStatus status;
    FrameXML_CreateFrame(XMLTree_GetRoot(tree.get()),nullptr,&status);
    tree.reset();
    RegionLua("assert(NativeTextureXML.Fill == NativeTextureXMLFill); assert(NativeTextureXMLFill:GetParent() == NativeTextureXML)");
    auto xml=Draw(device,root);
    Check(Ink(xml)>100 && status.IsEmpty(),"XML-created texture region did not render");
    Equal(xml,Draw(device,root));
    for (const char* invalid : {"<Texture horizTile='true'/>","<Texture><TexCoords left='nan'/></Texture>",
                                "<Texture><TexCoords><Rect ULx='10001'/></TexCoords></Texture>"}) {
        tree.reset(XMLTree_Load(invalid,std::strlen(invalid)));
        const auto used=CSimpleTexture::s_allocator.m_dataUsed;
        Reject([&]{LoadXML_Texture(XMLTree_GetRoot(tree.get()),parent,&status);});
        Check(CSimpleTexture::s_allocator.m_dataUsed==used,"Rejected texture XML allocated a region");
    }
    tree.reset();
    delete parent;
    RegionLua(R"(
        assert(NativeTextureParent == nil and NativeTextureParentImage == nil and NativeTextureXMLFill == nil)
        assert(not pcall(StaleTexture.GetTexture,StaleTexture))
        StaleTexture=nil; collectgarbage('collect')
    )");
    Check(CSimpleTexture::s_count==before && !CSimpleTexture::s_allocator.m_dataUsed,
          "Texture region teardown retained pooled objects");
    Check(!Ink(Draw(device,root)),"Texture XML destruction retained batches");
    FrameXML_FreeHashNodes();
    std::cout << "frame-texture-pool-growth-file-solid-atlas-desaturate-lua-xml-template-uv-alpha-hide-retained-teardown=passed source="
              << (original?"original":"authored") << '\n';
}
