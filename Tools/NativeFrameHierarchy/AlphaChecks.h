// Original Frame alpha APIs and copied region colors, exercised through actual
// root OnUpdate/collection/rendering. Authored pixels are not login evidence.
#include <array>
struct AlphaRenderGuard {
    CSimpleFrame* frame;
    int calls = 0;
    static void Run(void* value) {
        auto& self = *static_cast<AlphaRenderGuard*>(value);
        ++self.calls;
        Reject([&] { self.frame->SetFrameAlpha(42); });
        Reject([&] { self.frame->UpdateAlpha(); });
        RegionLua("assert(not pcall(AlphaParent.SetAlpha,AlphaParent,.2)); "
                  "assert(not pcall(AlphaParent.GetAlpha,AlphaParent)); "
                  "assert(not pcall(AlphaParent.GetEffectiveAlpha,AlphaParent))");
    }
};
static void AlphaChecks(LegacyDevice& device, CSimpleTop& root, CSimpleFont& font) {
    auto* parent = Create_SimpleFrame(nullptr);
    parent->SetName("AlphaParent");
    parent->SetSize(.8f, .35f);
    parent->SetPoint(FRAMEPOINT_BOTTOMLEFT, &root, FRAMEPOINT_BOTTOMLEFT, .05f, .05f, 1);
    auto* child = Create_SimpleFrame(parent);
    child->SetName("AlphaChild");
    child->SetAllPoints(parent, 1);
    auto* texture = Frame::Native::CreateTexture(child, 2, true);
    texture->SetSize(.3f, .25f);
    texture->SetPoint(FRAMEPOINT_BOTTOMLEFT, child, FRAMEPOINT_BOTTOMLEFT, .01f, .01f, 1);
    Check(texture->SetTexture(CImVector(0, 255, 0, 255)), "Alpha texture creation failed");
    auto* text = Frame::Native::CreateFontString(child, 2, true);
    text->SetName("AlphaText");
    text->SetSize(.4f, .25f);
    text->SetPoint(FRAMEPOINT_BOTTOMRIGHT, child, FRAMEPOINT_BOTTOMRIGHT, 0, 0, 1);
    text->SetFontObject(&font);
    RegionLua("AlphaText:SetText('AV fade'); AlphaText:SetTextColor(1,0,0,1)");
    const auto baseline = Draw(device, root);
    auto channels = [](const std::vector<CImVector>& pixels) {
        std::array<uint64_t, 2> result{};
        for (const auto& pixel : pixels) { result[0] += pixel.r; result[1] += pixel.g; }
        return result;
    };
    const auto full = channels(baseline);
    Check(full[0] > 100 && full[1] > 100, "Alpha fixture needs both text and texture pixels");
    RegionLua(R"(
        local f,c = AlphaParent,AlphaChild
        assert(f:GetAlpha()==1 and c:GetEffectiveAlpha()==1)
        for _,bad in ipairs({0/0,math.huge,-math.huge,1e100,{},false,'invalid'}) do
            assert(not pcall(f.SetAlpha,f,bad) and f:GetAlpha()==1)
        end
        assert(not pcall(f.SetAlpha,f) and f:GetAlpha()==1)
        assert(not pcall(f.SetAlpha,{[0]=f[0]},.5))
        assert(not pcall(f.GetAlpha,{[0]=f[0]}))
        assert(not pcall(f.GetEffectiveAlpha,AlphaText))
        f:SetAlpha(-1); assert(f:GetAlpha()==0 and c:GetEffectiveAlpha()==0)
        f:SetAlpha(2); assert(f:GetAlpha()==1 and c:GetEffectiveAlpha()==1)
        AlphaUpdates,AlphaNext=0,1
        f:SetScript('OnUpdate',function(self)
            self:SetAlpha(AlphaNext)
            assert(self:GetAlpha()==math.floor(AlphaNext*255)/255)
            assert(not pcall(self.Hide,self) and not pcall(self.SetParent,self,nil))
            assert(not pcall(CreateFrame,'Frame','RejectedAlphaCallback'))
            AlphaUpdates=AlphaUpdates+1
        end)
    )");
    // Consecutive nonopaque changes expose stale copied colors after the first
    // implicit-white transition. Restoration also exercises color-count removal.
    for (const char* alpha : {".75", ".5", ".25", "0", ".5", "1"}) {
        RegionLua((std::string("AlphaNext=") + alpha).c_str());
        const auto pixels = Draw(device, root);
        const auto faded = channels(pixels);
        const double ratio = parent->m_alpha / 255.0;
        for (size_t channel = 0; channel < full.size(); ++channel)
            Check(std::fabs(double(faded[channel]) / full[channel] - ratio) < .035,
                  "Consecutive Frame alpha did not update texture/text pixels");
        Equal(pixels, Draw(device, root));
    }
    Equal(baseline, Draw(device, root));
    RegionLua("assert(AlphaUpdates>=12); AlphaParent:SetScript('OnUpdate',nil); "
              "AlphaChild:SetAlpha(.5); AlphaParent:SetAlpha(.5); "
              "assert(AlphaChild:GetAlpha()==127/255 and AlphaChild:GetEffectiveAlpha()==63/255)");
    Check(child->m_parentAlpha == 127 && texture->m_colors[0].a == 63 && text->m_colors[0].a == 63,
          "Parent/child/region alpha did not preserve original integer quantization");
    const auto inherited = channels(Draw(device, root));
    for (size_t channel = 0; channel < full.size(); ++channel)
        Check(std::fabs(double(inherited[channel]) / full[channel] - 63.0/255) < .035,
              "Inherited alpha did not reach texture/text pixels");
    // Local region alpha remains independent of the Frame alpha factors.
    RegionLua("AlphaText:SetTextColor(1,0,0,.5); AlphaParent:SetAlpha(1); AlphaChild:SetAlpha(1)");
    // Frame alpha truncates; FrameScript_GetColor rounds .5 to 128 instead.
    Check(text->m_colors[0].a == 128 && texture->m_colorCount == 0,
          "Frame alpha restoration lost local region alpha or implicit white");
    RegionLua("AlphaText:SetTextColor(1,0,0,1)");
    Equal(baseline, Draw(device, root));
    {
        Frame::Native::CollectScope collecting;
        Reject([&] { parent->SetFrameAlpha(42); });
        Reject([&] { parent->UpdateAlpha(); });
        RegionLua("assert(not pcall(AlphaParent.SetAlpha,AlphaParent,.2)); assert(AlphaParent:GetAlpha()==1)");
    }
    {
        CRenderBatch batch;
        AlphaRenderGuard guard{parent};
        batch.QueueCallback(&AlphaRenderGuard::Run, &guard);
        batch.Finish();
        CSimpleRender::DrawBatch(&batch);
        Check(guard.calls == 1 && parent->m_alpha == 255, "Render-time alpha guard failed");
    }
    bool foreign = false;
    std::thread thread([&] { try { parent->SetFrameAlpha(42); } catch (const std::logic_error&) { foreign = true; } });
    thread.join();
    Check(foreign && parent->m_alpha == 255, "Foreign alpha mutation changed a live Frame");
    Equal(baseline, Draw(device, root));
    RegionLua("StaleAlpha=AlphaParent");
    delete parent;
    RegionLua("assert(not pcall(StaleAlpha.GetAlpha,StaleAlpha)); "
              "assert(not pcall(StaleAlpha.GetEffectiveAlpha,StaleAlpha)); "
              "assert(not pcall(StaleAlpha.SetAlpha,StaleAlpha,.5)); "
              "StaleAlpha,AlphaNext,AlphaUpdates=nil,nil,nil; collectgarbage('collect')");
    Check(!Ink(Draw(device, root)), "Alpha teardown retained region pixels");
    std::cout << "frame-alpha-update-inheritance-clamp-texture-text-repeat-guards-teardown=passed\n";
}
