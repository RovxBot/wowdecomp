// Authored texture region/XML/Lua coverage; optional original resource bytes.
static Bytes TextureFixtureBlp() {
    Bytes bytes(148);
    auto word = [&](size_t offset, uint32_t value) {
        for (unsigned i=0;i<4;++i) bytes[offset+i] = uint8_t(value >> (i*8));
    };
    word(0,0x32504c42);word(4,1);bytes[8]=3;bytes[9]=8;
    word(12,8);word(16,8);word(20,148);word(84,256);
    for (unsigned y=0;y<8;++y) for(unsigned x=0;x<8;++x) {
        const uint8_t pixel[]{uint8_t(y<4?0:255),uint8_t(x<4?0:255),uint8_t(x<4?255:0),255};
        bytes.insert(bytes.end(),pixel,pixel+4);
    }
    return bytes;
}
static void TextureChecks(LegacyDevice& device, CSimpleTop& root, bool original) {
    const int before = CSimpleTexture::s_count;
    auto* parent = Create_SimpleFrame(nullptr);
    parent->SetName("NativeTextureParent");
    parent->SetSize(.5f,.4f);
    parent->SetPoint(FRAMEPOINT_BOTTOMLEFT,&root,FRAMEPOINT_BOTTOMLEFT,.1f,.1f,1);
    RegionLua(R"(
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
    RegionLua("assert(NativeTextureParentImage:SetDesaturated(true)); assert(NativeTextureParentImage:IsDesaturated())");
    auto gray=Draw(device,root);
    Check(std::any_of(gray.begin(),gray.end(),[](const CImVector& p){return p.r>20 && std::abs(int(p.r)-p.g)<=2 && std::abs(int(p.r)-p.b)<=2;}),
          "Texture desaturation shader did not execute");
    RegionLua("NativeTextureParentImage:SetDesaturated(false)");
    Equal(solid,Draw(device,root));
    const char* filename=original ? "Interface\\Glues\\LoadingBar\\Loading-BarFill.blp" : "fixture.blp";
    Check(texture->SetTexture(filename,0,0,GxTex_Nearest,ImageMode_Normal),"File texture replacement failed");
    auto pixels=Draw(device,root);
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
    delete parent;
    RegionLua(R"(
        assert(NativeTextureParent == nil and NativeTextureParentImage == nil and NativeTextureXMLFill == nil)
        assert(not pcall(StaleTexture.GetTexture,StaleTexture))
        StaleTexture=nil; collectgarbage('collect')
    )");
    Check(CSimpleTexture::s_count==before && !CSimpleTexture::s_allocator.m_dataUsed,
          "Texture region teardown retained pooled objects");
    Check(!Ink(Draw(device,root)),"Texture XML destruction retained batches");
    std::cout << "frame-texture-pool-file-solid-desaturate-lua-xml-uv-alpha-hide-retained-teardown=passed source="
              << (original?"original":"authored") << '\n';
}
