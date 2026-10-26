#include "Engine/Source/Frame/Portable/Runtime.h"
#include "Engine/Source/Frame/CLayoutFrame.h"
#include "Engine/Source/Frame/CFramePoint.h"
#include "Engine/Source/FrameXML/XMLTree.h"
#include "Engine/Source/FrameXML/LoadXML.h"
#include "Engine/Source/Base/Coordinate.h"
#include "Engine/Source/Base/Status.h"
#include "Engine/Source/Os/Portable/Runtime.h"
#include "Engine/Source/Tempest/C2Vector.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
using namespace Frame::Native;
static void Check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
static void Near(float actual, float expected) {
    if (!(std::fabs(actual-expected)<0.00001f))
        throw std::runtime_error("layout coordinate mismatch: actual="+std::to_string(actual)+" expected="+std::to_string(expected));
}
template<class F> static void Reject(F f) {
    bool rejected=false;
    try { f(); } catch (const std::logic_error&) { rejected=true; }
    Check(rejected,"invalid layout operation accepted");
}
struct Layout : CLayoutFrame {
    CLayoutFrame* parent=nullptr;
    CLayoutFrame* named=nullptr;
    bool loaded=true, checkCallback=false;
    int changed=0;
    CLayoutFrame* GetLayoutParent() override { return parent; }
    CLayoutFrame* GetLayoutFrameByName(const char* name) override { return std::string(name)=="root" ? named : nullptr; }
    int32_t IsObjectLoaded() override { return loaded; }
    void OnFrameSizeChanged(const CRect& old) override {
        ++changed;
        if (checkCallback) Reject([&]{ DestroyLayout(); });
        CLayoutFrame::OnFrameSizeChanged(old);
    }
};
// A layout root owns its screen rectangle; no synthetic replacement solver.
static void Root(Layout& root, float w=0.8f, float h=0.6f) {
    root.m_rect.minX=0;root.m_rect.minY=0;
    root.m_rect.maxX=w;root.m_rect.maxY=h;
    root.m_layoutFlags|=0x100;
}
static void Rect(CLayoutFrame& frame,float l,float b,float r,float t) {
    CRect rect;
    Check(frame.GetRect(&rect),"layout rectangle missing");
    Near(rect.minX,l);Near(rect.minY,b);Near(rect.maxX,r);Near(rect.maxY,t);
}
static void Anchors() {
    Layout root;Root(root);
    {
        Layout frame;frame.checkCallback=true;
        Check(frame.m_layoutFlags==0 && !frame.HasAnyFramepoints(),"uninitialized native layout flags");
        frame.SetSize(0.2f,0.1f);
        frame.SetPoint(FRAMEPOINT_TOPLEFT,&root,FRAMEPOINT_TOPLEFT,0.05f,-0.04f,1);
        CLayoutFrame::ResizePending();
        Rect(frame,0.05f,0.46f,0.25f,0.56f);
        Near(frame.CenterX(),0.15f);Near(frame.CenterY(),0.51f);
        C2Vector hit;hit.x=0.15f;hit.y=0.5f;
        Check(frame.PtInFrameRect(hit),"inclusive frame hit bounds");
        hit.x=0.01f;
        Check(!frame.PtInFrameRect(hit),"outside frame hit bounds");
        int changed=frame.changed;
        frame.SetPoint(FRAMEPOINT_TOPLEFT,&root,FRAMEPOINT_TOPLEFT,0.05f,-0.04f,1);
        CLayoutFrame::ResizePending();Check(frame.changed==changed,"unchanged anchor resized");
        frame.SetLayoutScale(2,true);CLayoutFrame::ResizePending();
        Rect(frame,0.1f,0.32f,0.5f,0.52f);
        frame.SetLayoutDepth(0.1f,true);Near(frame.m_layoutDepth,0.2f);
        frame.SetClampedToScreen(1);
        frame.SetPoint(FRAMEPOINT_TOPLEFT,&root,FRAMEPOINT_TOPLEFT,-0.1f,0.1f,1);
        CLayoutFrame::ResizePending();Rect(frame,0,0.4f,0.4f,0.6f);
        frame.SetProtected();Check(frame.IsProtected() && root.IsProtected(),"anchor protection propagation");
    }
    Check(root.m_resizeList.Head()==nullptr,"destroyed dependent kept a resize registration");
    {
        Layout center;center.SetSize(0.2f,0.1f);
        center.SetPoint(FRAMEPOINT_CENTER,&root,FRAMEPOINT_CENTER,0,0,1);
        CLayoutFrame::ResizePending();Rect(center,0.3f,0.25f,0.5f,0.35f);
        Check(center.FlattenFrame(&root,0,0,0.02f,-0.01f),"flatten failed");
        Rect(center,0.32f,0.24f,0.52f,0.34f);
        Check(center.UnflattenFrame(&root),"unflatten failed");
        Rect(center,0.32f,0.24f,0.52f,0.34f);
    }
    // All nine point consumers execute their original X/Y switches.
    for (uint32_t i=0;i<FRAMEPOINT_NUMPOINTS;++i) {
        auto p=static_cast<FRAMEPOINT>(i);
        CFramePoint point(&root,p,0.01f,-0.02f);
        Near(point.X(1),(i%3)*0.4f+0.01f);
        Near(point.Y(1),(2-i/3)*0.3f-0.02f);
        point.MarkUnused();Check(point.X(1)==CFramePoint::UNDEFINED,"unused point resolved");
    }
}
static void Dependencies() {
    auto root=std::make_unique<Layout>();Root(*root);
    Layout middle,child;
    middle.SetAllPoints(root.get(),1);child.SetAllPoints(&middle,1);
    CLayoutFrame::ResizePending();Rect(child,0,0,0.8f,0.6f);
    float w=0,h=0;child.GetSize(w,h,0);Near(w,0.8f);Near(h,0.6f);
    Check(child.IsResizeDependency(root.get()),"transitive anchor dependency");
    Reject([&]{root->SetAllPoints(&child,1);});
    middle.SetDeferredResize(1);
    Root(*root,0.7f,0.5f);root->OnFrameSizeChanged(root->m_rect);
    CLayoutFrame::ResizePending();Rect(child,0,0,0.7f,0.5f);
    // Child resolving the deferred relative forces the actual original resize.
    Check(!(middle.m_layoutFlags&0x200),"deferred relative not resolved");
    CFramePoint standalone(root.get(),FRAMEPOINT_CENTER,0,0);
    root.reset();CLayoutFrame::ResizePending();
    Check(standalone.X(1)==CFramePoint::UNDEFINED,"standalone point retained destroyed origin");
    Check(!middle.HasAnyFramepoints() && middle.m_resizeList.Head()!=nullptr,"destroyed origin anchor retained");
    CRect rect;Check(!middle.GetRect(&rect),"destroyed anchor kept stale rectangle");
    Check(!child.GetRect(&rect),"transitive invalidation retained stale rectangle");
    // Unloaded pending objects stay linked until loaded, or unlink on destruction.
    Layout source;Root(source);
    { Layout unloaded;unloaded.loaded=false;unloaded.SetAllPoints(&source,1);
      CLayoutFrame::ResizePending();Check(unloaded.IsResizePending(),"unloaded pending frame removed");
      unloaded.loaded=true;CLayoutFrame::ResizePending();Rect(unloaded,0,0,0.8f,0.6f); }
    { Layout unloaded;unloaded.loaded=false;unloaded.SetAllPoints(&source,1); }
    CLayoutFrame::ResizePending();
}
static void InvalidInputs() {
    Layout root,frame;Root(root);frame.SetSize(0.2f,0.1f);
    frame.SetPoint(FRAMEPOINT_BOTTOMLEFT,&root,FRAMEPOINT_BOTTOMLEFT,0,0,1);
    CLayoutFrame::ResizePending();
    for (uint32_t i:{9u,255u,0xffffffffu}) {
        auto invalid=static_cast<FRAMEPOINT>(i);
        Reject([&]{frame.SetPoint(invalid,&root,FRAMEPOINT_CENTER,0,0,1);});
        Reject([&]{frame.SetPoint(FRAMEPOINT_CENTER,&root,invalid,0,0,1);});
        Reject([&]{frame.GetFramePoint(invalid);});
        float x;Reject([&]{frame.GetFramePointX(&invalid,1,x);});
    }
    Reject([&]{frame.SetPoint(FRAMEPOINT_CENTER,nullptr,FRAMEPOINT_CENTER,0,0,1);});
    Reject([&]{frame.SetAllPoints(&frame,1);});
    float x;Reject([&]{frame.GetFramePointY(nullptr,1,x);});
    for (float f:{std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}) {
        Reject([&]{frame.SetSize(f,1);});Reject([&]{frame.SetLayoutDepth(f,true);});
        Reject([&]{frame.SetLayoutScale(f,true);});
        Reject([&]{frame.SetPoint(FRAMEPOINT_CENTER,&root,FRAMEPOINT_CENTER,f,0,1);});
    }
    Reject([&]{frame.SetWidth(-1);});Reject([&]{frame.SetLayoutScale(0,true);});
    Rect(frame,0,0,0.2f,0.1f);
    bool foreign=false;
    std::thread thread([&]{try { frame.SetWidth(0.5f); } catch(const std::logic_error&) { foreign=true; }});
    thread.join();Check(foreign,"foreign layout mutation accepted");Near(frame.GetWidth(),0.2f);
    Reject([]{RequireNoObjects();});
    Reject([]{FrameXML::Native::RequireNoConsumers();});
    // Bounded dependency depth, including repeated anchors, without exponential walks.
    std::vector<std::unique_ptr<Layout>> chain;
    CLayoutFrame* previous=&root;bool bounded=false;
    for(int i=0;i<2050;++i) {
        auto next=std::make_unique<Layout>();
        try { next->SetAllPoints(previous,0); } catch(const std::invalid_argument&) { bounded=true;break; }
        previous=next.get();chain.push_back(std::move(next));
    }
    Check(chain.size()==2047 && bounded,"original HTML anchor depth or its bound changed");
    while(!chain.empty()) chain.pop_back();
}
static void XML() {
    for(float aspect:{4.0f/3.0f,16.0f/9.0f}) {
        CoordinateSetAspectRatio(aspect);
        float screenW=NDCToDDCWidth(1),screenH=NDCToDDCHeight(1);
        Layout root;Root(root,screenW,screenH);
        Layout frame;frame.parent=&root;frame.named=&root;
        std::string source="<Frame><Size x='256' y='128'/><Anchors><Anchor point='TOPLEFT' relativeTo='root' x='32' y='-16'/></Anchors></Frame>";
        auto* tree=XMLTree_Load(source.data(),static_cast<uint32_t>(source.size()));Check(tree,"layout XML tree");
        CStatus status;frame.LoadXML(XMLTree_GetRoot(tree),&status);XMLTree_Free(tree);
        CLayoutFrame::ResizePending();
        float unit=screenW/(aspect*0.75f*1024);
        Rect(frame,32*unit,screenH-144*unit,288*unit,screenH-16*unit);
        source="<Frame><Size><RelDimension x='0.2' y='0.1'/></Size><Anchors><Anchor point='CENTER'><Offset><RelDimension x='0.05' y='-0.02'/></Offset></Anchor></Anchors></Frame>";
        tree=XMLTree_Load(source.data(),static_cast<uint32_t>(source.size()));frame.MarkFramePointsUnused();
        frame.LoadXML(XMLTree_GetRoot(tree),&status);XMLTree_Free(tree);CLayoutFrame::ResizePending();
        Rect(frame,screenW/2-0.05f,screenH/2-0.07f,screenW/2+0.15f,screenH/2+0.03f);
        source="<Frame setAllPoints='true'/>";
        tree=XMLTree_Load(source.data(),static_cast<uint32_t>(source.size()));frame.LoadXML(XMLTree_GetRoot(tree),&status);
        XMLTree_Free(tree);CLayoutFrame::ResizePending();Rect(frame,0,0,screenW,screenH);
    }
    CoordinateSetAspectRatio(4.0f/3.0f);
}
static void Capacity() {
    // Actual signed Glue creation exceeded the initial 1,024-object surface.
    // This metadata fixture has no anchors, scripts, images or replacement UI.
    std::vector<std::unique_ptr<Layout>> frames;
    for (size_t i = 0; i < 8192; ++i) frames.push_back(std::make_unique<Layout>());
    Reject([] { auto excessive = std::make_unique<Layout>(); });
    frames.pop_back();
    frames.push_back(std::make_unique<Layout>());
    frames.clear();
    RequireNoObjects();
}
int main() {
    try {
        for(int cycle=0;cycle<3;++cycle) {
            FrameScript::Native::Runtime scripts([](auto,const auto&,auto&){return false;});
            FrameXML::Native::Runtime xml;
            { Frame::Native::Runtime layout;
              Anchors();Dependencies();InvalidInputs();XML();Capacity();CLayoutFrame::ResizePending();RequireNoObjects(); }
            FrameXML::Native::RequireNoConsumers();
            Check(CFramePoint::s_framePointHeap.m_dataUsed==0 && !CFramePoint::s_framePointHeap.m_blockList,"layout pool not cleared");
            std::cout<<"Original layout/anchor/XML cycle "<<cycle+1<<" passed\n";
        }
        return 0;
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n';return 1; }
}
