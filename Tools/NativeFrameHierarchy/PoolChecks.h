#include "Engine/Source/Frame/Portable/Storage.h"
#include "Engine/Source/FrameXML/FrameXML.h"

struct ThrowingPooledFrame final : CSimpleFrame {
    explicit ThrowingPooledFrame(CSimpleFrame* parent) : CSimpleFrame(parent) {
        throw std::runtime_error("Authored pooled constructor failure");
    }
};
static void PoolChecks(LegacyDevice& device, CSimpleTop& root, CSimpleFont& font,
                       Frame::Native::Fonts& owner) {
    auto& frames = CSimpleFrame::s_allocator;
    auto& strings = CSimpleFontString::s_allocator;
    Check(!frames.m_dataUsed && !strings.m_dataUsed, "Frame pools started with live slots");
    // A slot awaiting construction must also prevent owner shutdown.
    void* raw = Frame::Native::AllocateRegionStorage(frames, sizeof(CSimpleFrame), alignof(CSimpleFrame));
    Reject([&]{owner.CheckCanClose();});
    Frame::Native::FreeRegionStorage(frames, raw);
    Reject([&]{Frame::Native::AllocateRegionStorage(frames, sizeof(CSimpleFrame) + 1, alignof(CSimpleFrame));});
    Reject([&]{Frame::Native::AllocateRegionStorage(frames, sizeof(CSimpleFrame), 64);});
    owner.CheckCanClose();

    std::vector<CSimpleFrame*> parents;
    for (uint32_t i = 0; i <= frames.m_dataPerBlock; ++i) {
        auto* frame = Create_SimpleFrame(nullptr);
        Check(uintptr_t(frame) % alignof(CSimpleFrame) == 0, "Unaligned pooled Frame");
        parents.push_back(frame);
    }
    Check(frames.m_dataUsed == parents.size() && frames.m_blockList->m_next,
          "Frame pool did not grow across its original block boundary");
    auto* parent = parents.front();
    parent->SetSize(0.5f, 0.4f);
    parent->SetPoint(FRAMEPOINT_BOTTOMLEFT, &root, FRAMEPOINT_BOTTOMLEFT, 0.1f, 0.1f, 1);
    std::vector<CSimpleFontString*> text;
    for (uint32_t i = 0; i <= strings.m_dataPerBlock; ++i) {
        auto* string = Frame::Native::CreateFontString(parent, 2, true);
        Check(uintptr_t(string) % alignof(CSimpleFontString) == 0, "Unaligned pooled FontString");
        text.push_back(string);
    }
    Check(strings.m_dataUsed == text.size() && strings.m_blockList->m_next,
          "FontString pool did not grow across its original block boundary");
    auto* retired = text.front();
    delete retired;
    text.front() = Frame::Native::CreateFontString(parent, 2, true);
    Check(text.front() == retired && !text.front()->m_string && !text.front()->m_text,
          "FontString slot reuse retained the prior object");
    text.front()->SetAllPoints(parent, 1);
    text.front()->SetFontObject(&font);
    text.front()->SetText("AV pooled Frame", 0);
    auto pixels = Draw(device, root);
    Check(Ink(pixels) > 4, "Pooled FontString did not render through the real hierarchy");
    Equal(pixels, Draw(device, root));
    // Heap subclasses and exact heap objects coexist in both ownership directions.
    auto* heap = new CSimpleFrame(parent);
    auto* pooledChild = Create_SimpleFrame(heap);
    new GuardedText(pooledChild);
    new CSimpleFontString(parent, 2, true);
    Frame::Native::CreateFontString(heap, 2, true);

    const auto usedFrames = frames.m_dataUsed, usedStrings = strings.m_dataUsed;
    Reject([&]{Create_SimpleFrame(reinterpret_cast<CSimpleFrame*>(uintptr_t(1)));});
    Reject([&]{Frame::Native::CreateFontString(parent, UINT32_MAX, true);});
    Check(frames.m_dataUsed == usedFrames && strings.m_dataUsed == usedStrings,
          "Factory preflight consumed a pool slot");
    static_assert(sizeof(ThrowingPooledFrame) == sizeof(CSimpleFrame), "Authored throwing Frame size");
    bool threw = false;
    try { Frame::Native::ConstructRegion<ThrowingPooledFrame>(frames, parent); }
    catch (const std::runtime_error&) { threw = true; }
    Check(threw && frames.m_dataUsed == usedFrames, "Constructor failure leaked a pool slot");
    bool foreign = false;
    std::thread thread([&]{try { Create_SimpleFrame(parent); }
        catch (const std::logic_error&) { foreign = true; }});
    thread.join(); Check(foreign && frames.m_dataUsed == usedFrames, "Foreign pooled factory accepted");
    // Deleting the parent recursively retires pooled/heap children, text and batches.
    Frame::Native::DestroyLater(parent);
    Frame::Native::DestroyLater(pooledChild);
    root.OnLayerUpdate(0.016f);
    for (size_t i = 1; i < parents.size(); ++i) delete parents[i];
    Check(!frames.m_dataUsed && !strings.m_dataUsed && !CSimpleFrame::s_frameCount,
          "Recursive mixed destruction left pool slots");
    Check(!Ink(Draw(device, root)), "Deleted pooled text remained in root batches");
    owner.CheckCanClose();
    std::cout << "frame-pooled-allocation-growth-reuse-constructor-rollback-mixed-delete=passed frame-slots="
              << parents.size() << " string-slots=" << text.size() << '\n';
}
