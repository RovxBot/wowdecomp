// Authored pixels through the original ScrollFrame and retained Frame batches.
#include "Engine/Source/Frame/CSimpleScrollFrame.h"
#include "Engine/Source/Frame/CSimpleTexture.h"
#include <algorithm>
namespace ScrollRenderChecks {
static void Check(bool good, const char* message) { if (!good) throw std::runtime_error(message); }
template<class F> static void Reject(F action) {
    bool rejected = false;
    try { action(); } catch (const std::logic_error&) { rejected = true; }
    Check(rejected, "Unsafe scroll render operation admitted");
}
struct Probe final : CSimpleFrame {
    CSimpleScrollFrame* scroll;
    bool fail = false, collectFail = false;
    unsigned calls = 0;
    std::vector<unsigned>* order;
    unsigned id;
    Probe(CSimpleFrame* parent, CSimpleScrollFrame* owner, std::vector<unsigned>& sequence, unsigned value)
        : CSimpleFrame(parent), scroll(owner), order(&sequence), id(value) {}
    void OnFrameRender(CRenderBatch* batch, uint32_t layer) override {
        CSimpleFrame::OnFrameRender(batch, layer);
        if (collectFail) throw std::runtime_error("Injected scroll collection failure");
        if (layer == 4) batch->QueueCallback(+[](void* raw) {
            auto& self = *static_cast<Probe*>(raw);
            ++self.calls; self.order->push_back(self.id);
            CRect cached{};
            Check(self.GetHitRect(cached) != 0, "Cached scroll child hit rectangle unavailable");
            Reject([&] { self.SetWidth(.2f); });
            Reject([&] { self.Hide(); });
            Reject([&] { self.SetParent(nullptr); });
            Reject([&] { self.CSimpleFrame::OnFrameRender(); });
            Reject([&] { CSimpleScrollFrame::RenderScrollChild(self.scroll); });
            Reject([&] { self.scroll->SetScrollChild(nullptr); });
            auto* scrollChild = self.scroll->m_scrollChild;
            Reject([&] { self.scroll->UnparentFrame(scrollChild); });
            Check(self.scroll->m_scrollChild == scrollChild, "Rejected unparent changed the scroll child");
            const auto offset = self.scroll->m_scrollOffset;
            Reject([&] { self.scroll->SetVerticalScroll(.1f); });
            Check(self.scroll->m_scrollOffset.y == offset.y, "Rejected scroll mutation changed its offset");
            Reject([&] { Frame::Native::RequireDestroyable(); });
            Reject([&] { Frame::Native::RequireTreeMutation(); });
            Reject([&] { CRenderBatch nested; });
            for (auto* current : self.m_renderBatch) if (current) {
                Reject([&] { current->Clear(); });
                Reject([&] { current->Finish(); });
                Reject([&] { CSimpleRender::DrawBatch(current); });
            }
            if (self.fail) throw std::runtime_error("Injected scroll child draw failure");
        }, this);
    }
};
static CSimpleTexture* Solid(CSimpleFrame* frame, uint32_t layer, const CImVector& color) {
    auto* texture = Frame::Native::CreateTexture(frame, layer, true);
    texture->SetAllPoints(frame, 1);
    Check(texture->SetTexture(color), "Authored scroll solid texture unavailable");
    return texture;
}
static std::vector<CImVector> Pixels(Gx::Native::LegacyDevice& device) {
    CiRect rect{}; rect.maxX = 320; rect.maxY = 240;
    TSGrowableArray<CImVector> pixels;
    device.DeviceReadPixels(rect, pixels);
    return {pixels.Ptr(), pixels.Ptr() + pixels.Count()};
}
static void Color(const std::vector<CImVector>& pixels, float x, float y, int r, int g, int b) {
    auto p = pixels[size_t(y * 400) * 320 + size_t(x * 400)];
    if (std::abs(int(p.r)-r) > 2 || std::abs(int(p.g)-g) > 2 || std::abs(int(p.b)-b) > 2)
        throw std::runtime_error("Scroll pixel at " + std::to_string(x) + "," + std::to_string(y) +
            " was " + std::to_string(p.r) + "," + std::to_string(p.g) + "," + std::to_string(p.b));
}
static void Run(CSimpleTop& root, Gx::Native::LegacyDevice& device) {
    device.RsPush();
    struct RestoreStates { Gx::Native::LegacyDevice& device; ~RestoreStates() { device.RsPop(); } } states{device};
    GxRsSet(GxRs_Lighting, 0); GxRsSet(GxRs_Fog, 0); GxRsSet(GxRs_Culling, 0);
    std::vector<unsigned> order;
    auto scroll = std::make_unique<CSimpleScrollFrame>(nullptr);
    scroll->SetSize(.4f, .3f);
    scroll->SetPoint(FRAMEPOINT_BOTTOMLEFT, &root, FRAMEPOINT_BOTTOMLEFT, .1f, .1f, 1);
    auto* content = new Probe(scroll.get(), scroll.get(), order, 1);
    content->SetSize(.6f, .5f);
    scroll->SetScrollChild(content);
    Solid(content, 0, CImVector(0,255,0,255));
    auto* nested = new CSimpleScrollFrame(content);
    nested->SetSize(.2f, .2f);
    nested->SetPoint(FRAMEPOINT_TOPLEFT, content, FRAMEPOINT_TOPLEFT, .3f, -.05f, 1);
    auto* inner = new Probe(nested, nested, order, 2);
    inner->SetSize(.3f, .3f);
    nested->SetScrollChild(inner);
    Solid(inner, 0, CImVector(255,0,0,255));
    auto* sibling = new Probe(content, scroll.get(), order, 3);
    sibling->SetSize(.1f, .1f);
    sibling->SetPoint(FRAMEPOINT_TOPLEFT, content, FRAMEPOINT_TOPLEFT, .05f, -.05f, 1);
    Solid(sibling, 0, CImVector(0,0,255,255));
    Solid(sibling, 4, CImVector(0,255,255,255));
    sibling->EnableDrawLayer(4);
    auto paint = [&] {
        root.OnLayerUpdate(.016f);
        GxXformSetViewport(0,1,0,1,.2f,.8f);
        GxSceneClear(GxBufferBit_Color, CImVector(0,0,0,255));
        order.clear(); root.OnLayerRender();
        return Pixels(device);
    };
    auto pixels = paint();
    Check(order == std::vector<unsigned>({1,2,3}), "Scroll layer/child callback order changed");
    Color(pixels,.3f,.2f,0,255,0); Color(pixels,.45f,.25f,0,0,255);
    Color(pixels,.2f,.3f,255,255,0); Color(pixels,.55f,.25f,0,0,0);
    Color(pixels,.05f,.2f,0,0,0); Color(pixels,.3f,.05f,0,0,0);
    auto repeated = paint();
    Check(pixels.size() == repeated.size() && !std::memcmp(pixels.data(),repeated.data(),pixels.size()*sizeof(CImVector)),
        "Retained scroll batches changed repeat pixels");
    float viewport[6]; GxXformViewport(viewport[0],viewport[1],viewport[2],viewport[3],viewport[4],viewport[5]);
    const auto projection = device.m_projection;
    const auto view = device.m_xforms[GxXform_View].m_mtx[device.m_xforms[GxXform_View].m_level];
    inner->fail = true;
    bool failed = false;
    try { root.OnLayerRender(); } catch (const std::runtime_error& e) { failed = std::string(e.what()) == "Injected scroll child draw failure"; }
    Check(failed, "Nested scroll failure did not propagate");
    float restored[6]; GxXformViewport(restored[0],restored[1],restored[2],restored[3],restored[4],restored[5]);
    Check(!std::memcmp(viewport,restored,sizeof(viewport)) && device.m_projection == projection &&
        device.m_xforms[GxXform_View].m_mtx[device.m_xforms[GxXform_View].m_level] == view,
        "Nested scroll failure did not restore projection/view/viewport");
    Frame::Native::RequireTreeMutation(); // Outer render guard must also unwind.
    inner->fail = false;
    auto recovered = paint();
    Check(recovered.size() == pixels.size() && !std::memcmp(pixels.data(),recovered.data(),pixels.size()*sizeof(CImVector)),
        "Scroll draw recovery changed pixels");
    inner->collectFail = true; inner->NotifyDrawLayerChanged(0);
    failed = false;
    try { root.OnLayerRender(); } catch (const std::runtime_error& e) { failed = std::string(e.what()) == "Injected scroll collection failure"; }
    Check(failed, "Scroll collection failure did not propagate");
    inner->collectFail = false;
    recovered = paint();
    Check(recovered.size() == pixels.size() && !std::memcmp(pixels.data(),recovered.data(),pixels.size()*sizeof(CImVector)),
        "Scroll collection recovery changed pixels");
    nested->Hide(); pixels = paint(); Color(pixels,.45f,.25f,0,255,0);
    nested->Show(); pixels = paint(); Color(pixels,.45f,.25f,0,0,255);
    // Move the nested viewport entirely beyond its parent's clipping rectangle.
    nested->MarkFramePointsUnused();
    nested->SetPoint(FRAMEPOINT_TOPLEFT,content,FRAMEPOINT_TOPLEFT,.5f,-.05f,1);
    pixels = paint(); Color(pixels,.45f,.25f,0,255,0);
    // A former scrolled subtree must retire its own batches before strata reuse.
    scroll->SetScrollChild(nullptr);
    content->SetParent(nullptr);
    paint();
    delete content;
    paint();
    Reject([&] { CSimpleScrollFrame::RenderScrollChild(scroll.get()); });
    bool foreign = false;
    std::thread thread([&] { try { CRect rect; scroll->GetHitRect(rect); } catch (const std::logic_error&) { foreign = true; } });
    thread.join(); Check(foreign, "Foreign cached scroll rectangle admitted");
    std::cout << "native-scroll-render-clipping-nesting-order-retention-guards-recovery=passed\n";
}
}
