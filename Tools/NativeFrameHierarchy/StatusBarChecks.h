// Original StatusBar range, texture, callbacks and retained fill geometry.
#include "Engine/Source/Frame/CSimpleStatusBar.h"
static void StatusBarChecks(LegacyDevice& device, CSimpleTop& root, bool original,
                            const std::string& snapshot) {
    const int frames = CSimpleFrame::s_frameCount;
    const int textures = CSimpleTexture::s_count;
    {
        std::vector<std::unique_ptr<CSimpleFrame>> pooled;
        for (unsigned i = 0; i < 11; ++i) {
            pooled.emplace_back(Create_SimpleStatusBar(nullptr));
            Check(uintptr_t(pooled.back().get()) % alignof(CSimpleStatusBar) == 0,
                  "StatusBar pool alignment");
        }
        Check(CSimpleStatusBar::s_allocator.m_dataUsed == 11, "StatusBar bypassed pool growth");
    }
    CStatus status;
    const char* source = R"(<Ui><StatusBar name="NativeBarTemplate" virtual="true"
      minValue="0" maxValue="100" defaultValue="50" orientation="HORIZONTAL">
      <Size x="200" y="80"/><Anchors><Anchor point="BOTTOMLEFT" x="64" y="64"/></Anchors>
      <BarTexture name="$parentFill"><Color r="1" g="0" b="0"/></BarTexture>
      <BarColor r="1" g="1" b="1"/>
      <Scripts><OnMinMaxChanged>self.ranges=(self.ranges or 0)+1</OnMinMaxChanged>
      <OnValueChanged>self.values=(self.values or 0)+1; self.last=value</OnValueChanged></Scripts>
    </StatusBar></Ui>)";
    std::unique_ptr<XMLTree, decltype(&XMLTree_Free)> tree(XMLTree_Load(source, std::strlen(source)), &XMLTree_Free);
    Check(bool(tree), "StatusBar XML parse");
    FrameXML_StoreHashNode(XMLTree_GetRoot(tree.get())->m_child, "NativeBarTemplate", nullptr, &status);
    tree.reset();
    RegionLua(R"(
      NativeBar=CreateFrame('StatusBar','NativeBar',nil,'NativeBarTemplate')
      assert(NativeBar:GetObjectType()=='StatusBar' and NativeBar:IsObjectType('Frame'))
      assert(NativeBar:GetValue()==50 and NativeBar:GetOrientation()=='HORIZONTAL')
      assert(NativeBar:GetStatusBarTexture()==NativeBarFill)
      assert(NativeBar:GetMinMaxValues()==0 and NativeBar.last==50)
      assert(not pcall(NativeBar.SetStatusBarTexture,NativeBar,{[0]=NativeBarFill[0]}))
      assert(not pcall(NativeBar.SetStatusBarTexture,NativeBar,NativeBar))
      assert(not pcall(NativeBar.SetValue,NativeBar,0/0))
      assert(not pcall(NativeBar.SetMinMaxValues,NativeBar,-1e12,1e12))
      assert(not pcall(NativeBar.SetOrientation,NativeBar,'INVALID'))
      local count=NativeBar.values
      NativeBar:SetValue(50); assert(NativeBar.values==count)
      NativeBar:SetValue(150); assert(NativeBar:GetValue()==100 and NativeBar.last==100)
      NativeBar:SetValue(-1); assert(NativeBar:GetValue()==0)
      NativeBar:SetValue(50)
      StaleBar=NativeBar; StaleBarFill=NativeBarFill
    )");
    auto* bar = static_cast<CSimpleStatusBar*>(CScriptObject::GetScriptObjectByName("NativeBar", CSimpleStatusBar::GetObjectType()));
    Check(bar && bar->m_barTexture, "StatusBar XML factory");
    auto horizontal = Draw(device, root);
    Equal(horizontal, Draw(device, root));
    auto red = [](const CImVector& p) { return p.r > 240 && p.g < 8 && p.b < 8; };
    const auto count = std::count_if(horizontal.begin(), horizontal.end(), red);
    Check(count > 500, "StatusBar horizontal fill missing");
    CRect frame, fill;
    Check(bar->GetRect(&frame) && bar->m_barTexture->GetRect(&fill), "StatusBar unresolved fill");
    auto near = [](float a, float b) { return std::fabs(a-b) < 0.0001f; };
    Check(near(fill.maxX-fill.minX, (frame.maxX-frame.minX)*.5f) &&
          near(fill.maxY, frame.maxY) && near(fill.minY, frame.minY), "StatusBar horizontal fraction");
    RegionLua(R"(
      local custom=NativeBar:CreateTexture()
      custom:SetTexture(1,0,0,1); custom:SetTexCoord(.2,.8,.1,.9)
      NativeBar:SetStatusBarTexture(custom)
      local x,y=custom:GetTexCoord()
      assert(math.abs(x-.2)<1e-6 and math.abs(y-.1)<1e-6)
    )");
    RegionLua("NativeBar:SetOrientation('VERTICAL'); NativeBar:SetValue(25); NativeBar:SetRotatesTexture(true)");
    auto vertical = Draw(device, root);
    Check(bar->m_barTexture->GetRect(&fill) && near(fill.maxY-fill.minY, (frame.maxY-frame.minY)*.25f) &&
          near(fill.minX, frame.minX) && near(fill.maxX, frame.maxX), "StatusBar vertical fraction");
    Check(std::count_if(vertical.begin(), vertical.end(), red) > 200 && Ink(vertical) < Ink(horizontal),
          "StatusBar vertical pixels");
    Equal(vertical, Draw(device, root));
    RegionLua("NativeBar:SetValue(0)");
    Check(!Ink(Draw(device, root)) && !bar->m_barTexture->m_visible, "Zero StatusBar retained fill");
    RegionLua("NativeBar:SetOrientation('HORIZONTAL'); NativeBar:SetValue(50); NativeBar:SetRotatesTexture(false)");
    Equal(horizontal, Draw(device, root));
    float width, height;
    bar->GetSize(width, height, 0); bar->SetSize(width*.5f, height*.5f);
    auto resized = Draw(device, root);
    Check(bar->GetRect(&frame) && bar->m_barTexture->GetRect(&fill) &&
          near(fill.maxX-fill.minX, (frame.maxX-frame.minX)*.5f) && Ink(resized) < Ink(horizontal),
          "Resized StatusBar retained old fill geometry");
    bar->SetSize(width, height); Equal(horizontal, Draw(device, root));
    Reject([&] { bar->SetOrientation(UINT32_MAX); });
    Reject([&] { bar->SetValue(std::numeric_limits<float>::infinity()); });
    Reject([&] { bar->SetBarTexture(bar->m_barTexture, 5); });
    {
        Frame::Native::CollectScope collect;
        Reject([&] { bar->SetValue(30); });
        RegionLua("assert(not pcall(NativeBar.SetValue,NativeBar,30))");
    }
    bool foreign = false;
    std::thread worker([&] { try { bar->SetValue(30); } catch (const std::logic_error&) { foreign = true; } });
    worker.join(); Check(foreign, "Foreign StatusBar mutation admitted");
    RegionLua(R"(
      NativeBar:SetScript('OnValueChanged',function(self,value)
        self.last=value
        self:SetStatusBarColor(1,1,1)
        assert(not pcall(self.SetStatusBarTexture,self,nil))
        assert(not pcall(CreateFrame,'Frame','InvalidBarCallback'))
      end)
      NativeBar:SetValue(60); assert(NativeBar.last==60)
      local other=CreateFrame('Button','OtherBarOwner')
      other:SetNormalTexture(other:CreateTexture())
      assert(not pcall(NativeBar.SetStatusBarTexture,NativeBar,other:GetNormalTexture()))
      NativeBar:SetStatusBarTexture(nil); assert(NativeBar:GetStatusBarTexture()==nil)
      assert(not pcall(StaleBarFill.GetTexture,StaleBarFill))
      NativeBar:SetRotatesTexture(true); assert(NativeBar:GetRotatesTexture()==1)
    )");
    delete CScriptObject::GetScriptObjectByName("OtherBarOwner", CSimpleFrame::GetObjectType());
    Check(bar->SetBarTexture(original ? "Interface\\Glues\\LoadingBar\\Loading-BarFill" : "fixture.blp", 2),
          "StatusBar file texture factory");
    Check(bar->m_barTexture && bar->m_barTexture->m_parent == bar && bar->m_rotatesTexture,
          "StatusBar filename ownership/rotation");
    RegionLua("NativeBar:SetScript('OnValueChanged',nil); NativeBar:SetValue(50); NativeBar:SetRotatesTexture(false)");
    auto filePixels = Draw(device, root); Equal(filePixels, Draw(device, root));
    Check(Ink(filePixels) > 100, "StatusBar file pixels missing");
    if (!snapshot.empty()) {
        const std::vector<std::pair<const char*, const std::vector<CImVector>*>> captures = {
            {".statusbar-horizontal.ppm", &horizontal}, {".statusbar-vertical.ppm", &vertical},
            {".statusbar-image.ppm", &filePixels}};
        for (const auto& capture : captures) {
            std::ofstream out(snapshot + capture.first, std::ios::binary); out << "P6\n640 480\n255\n";
            for (int y=479; y>=0; --y) for (int x=0; x<640; ++x) {
                const auto& p = (*capture.second)[y*640+x]; out.put(char(p.r)); out.put(char(p.g)); out.put(char(p.b));
            }
            Check(bool(out), "StatusBar capture");
        }
    }
    RegionLua("NativeBar:SetMinMaxValues(80,20); local lo,hi=NativeBar:GetMinMaxValues(); assert(lo==20 and hi==20 and NativeBar:GetValue()==20)");
    Check(bar->GetAnimValue() == 0 && !Ink(Draw(device, root)), "Degenerate StatusBar range");
    delete bar; FrameXML_FreeHashNodes();
    RegionLua("assert(not pcall(StaleBar.GetValue,StaleBar)); StaleBar=nil; StaleBarFill=nil");
    Check(CSimpleFrame::s_frameCount == frames && CSimpleTexture::s_count == textures &&
          !CSimpleStatusBar::s_allocator.m_dataUsed && !Ink(Draw(device,root)), "StatusBar retained slots/regions/pixels");
    std::cout << "statusbar=range-callbacks-horizontal-vertical-resize-texture-ownership\n";
}
