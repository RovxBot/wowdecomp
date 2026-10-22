// Authored movement through original root/layout consumers, separate from physical input.
#include "Engine/Source/Frame/CSimpleTitleRegion.h"
#include "Engine/Source/Frame/CFramePoint.h"
struct MovingFrame final : CSimpleFrame {
    int changes=0;
    bool fail=false, defer=false;
    MovingFrame():CSimpleFrame(nullptr) {}
    void OnFrameSizeChanged(float width,float height) override {
        ++changes;
        Reject([&]{Hide();});Reject([&]{Raise();});
        Reject([&]{Frame::Native::CreateTitleRegion(this);});
        Reject([&]{m_simpleTop->StopMoveOrResizeFrame();});
        Reject([&]{m_simpleTop->OnLayerUpdate(0.01f);});
        Reject([&]{Frame::Native::RequireDestroyable();});
        if(defer) Frame::Native::DestroyLater(this);
        if(fail) throw std::runtime_error("Authored movement resize failure");
        CSimpleFrame::OnFrameSizeChanged(width,height);
    }
};
static void MovementEvents(GlContext& context,LegacyDevice& device,CSimpleTop& root,CSimpleFont& font) {
    auto deliver=[&](OSINPUT id,int value=0,int x=128,int y=336) {
        return Frame::Native::DispatchInputEvent(context,OSEVENT{id,{value,x,y,678}});
    };
    auto near=[](float a,float b){return std::fabs(a-b)<0.00001f;};
    auto rect=[&](CSimpleFrame* frame) {CRect value{};Check(frame->GetRect(&value),"Movement rectangle unavailable");return value;};
    auto same=[&](const CRect& a,const CRect& b) {
        Check(near(a.minX,b.minX)&&near(a.minY,b.minY)&&near(a.maxX,b.maxX)&&near(a.maxY,b.maxY),"Movement rectangle changed unexpectedly");
    };
    root.CancelMouseInput();
    auto* frame=new MovingFrame;frame->SetFrameStrata(FRAME_STRATA_DIALOG);
    auto place=[&] {
        frame->MarkFramePointsUnused();
        frame->SetSize(NDCToDDCWidth(0.3f),NDCToDDCHeight(0.3f));
        frame->SetPoint(FRAMEPOINT_BOTTOMLEFT,&root,FRAMEPOINT_BOTTOMLEFT,NDCToDDCWidth(0.1f),NDCToDDCHeight(0.1f),1);
        CLayoutFrame::ResizePending();
    };
    place();frame->SetFrameFlag(1,1);frame->EnableEvent(SIMPLE_EVENT_MOUSE,20);
    auto* title=Frame::Native::CreateTitleRegion(frame);
    Check(title==Frame::Native::CreateTitleRegion(frame) && title->m_parent==frame,"Title owner/idempotence");
    title->SetSize(NDCToDDCWidth(0.3f),NDCToDDCHeight(0.08f));
    title->SetPoint(FRAMEPOINT_TOPLEFT,frame,FRAMEPOINT_TOPLEFT,0,0,1);
    auto* string=new CSimpleFontString(frame,2,1);
    string->SetSize(NDCToDDCWidth(0.2f),NDCToDDCHeight(0.1f));
    string->SetPoint(FRAMEPOINT_BOTTOMLEFT,frame,FRAMEPOINT_BOTTOMLEFT,0,0,1);
    string->SetFontObject(&font);string->SetText("AV",0);
    auto first=Draw(device,root);const auto original=rect(frame);
    // Title hit starts movement without ordinary widget capture; logical +X/-Y
    // reaches the original DDC geometry and the attached text batch.
    deliver(OS_INPUT_MOUSE_MOVE,0,128,300);deliver(OS_INPUT_MOUSE_DOWN,MOUSE_BUTTON_LEFT,128,300);
    Check(root.m_moveResizeFrame==frame && root.m_moveResizeReason==MOVERESIZE_REASON_TITLE && !root.m_mouseCapture,
          "Title click did not start original movement");
    Check(frame->m_frameFlags&0x1000,"Movement did not preserve user-placed flag");
    deliver(OS_INPUT_MOUSE_MOVE,0,192,276);
    auto moved=rect(frame);
    Check(near(moved.minX-original.minX,NDCToDDCWidth(0.1f)) && near(moved.minY-original.minY,NDCToDDCHeight(0.05f)),"Title movement DDC delta/Y conversion");
    Check(near(moved.maxX-moved.minX,original.maxX-original.minX) && near(moved.maxY-moved.minY,original.maxY-original.minY),"Title movement resized Frame");
    auto shifted=Draw(device,root);Check(std::memcmp(first.data(),shifted.data(),first.size()*sizeof(CImVector))!=0,"Movement did not move attached text pixels");Equal(shifted,Draw(device,root));
    deliver(OS_INPUT_MOUSE_UP,MOUSE_BUTTON_LEFT,192,276);
    Check(!root.m_moveResizeFrame && root.m_moveResizeReason==MOVERESIZE_REASON_NONE,"Title release retained movement");same(moved,rect(frame));
    deliver(OS_INPUT_MOUSE_MOVE,0,220,100);same(moved,rect(frame));
    // Outside-title clicks keep ordinary capture and leave geometry unchanged.
    place();deliver(OS_INPUT_MOUSE_MOVE);deliver(OS_INPUT_MOUSE_DOWN,MOUSE_BUTTON_LEFT);
    Check(root.m_mouseCapture==frame && !root.m_moveResizeFrame,"Body click started title movement");
    deliver(OS_INPUT_MOUSE_UP,MOUSE_BUTTON_LEFT);same(original,rect(frame));
    // Script reason is the original direct C++ entry, not a Lua factory. It
    // survives button release until explicit stop, and preserves scaled geometry.
    frame->SetFrameScale(1.5f,1);CLayoutFrame::ResizePending();
    deliver(OS_INPUT_MOUSE_MOVE);auto scaled=rect(frame);C2Vector cursor;root.GetCursorPosition(cursor);
    Check(root.StartMoveOrResizeFrame(frame,MOVERESIZE_REASON_SCRIPT,cursor.x,cursor.y,uint32_t(FRAMEPOINT_CENTER)),"Direct movement start");
    Reject([&]{root.StartMoveOrResizeFrame(frame,MOVERESIZE_REASON_SCRIPT,cursor.x,cursor.y,uint32_t(FRAMEPOINT_CENTER));});
    deliver(OS_INPUT_MOUSE_MOVE,0,160,312);auto scaledMoved=rect(frame);
    Check(near(scaledMoved.minX-scaled.minX,NDCToDDCWidth(0.05f)) && near(scaledMoved.minY-scaled.minY,NDCToDDCHeight(0.05f)),"Effective-scale movement delta");
    deliver(OS_INPUT_MOUSE_UP,MOUSE_BUTTON_LEFT,160,312);Check(root.m_moveResizeFrame==frame,"Script movement stopped on mouse-up");
    root.StopMoveOrResizeFrame();same(scaledMoved,rect(frame));frame->SetFrameScale(1,1);place();
    // All nine original points: fixed opposite edges, corners and center. Bounds
    // clamp resizing without moving the fixed edges, including minimum/maximum.
    for(uint32_t point=0;point<FRAMEPOINT_NUMPOINTS;++point) {
        place();deliver(OS_INPUT_MOUSE_MOVE);root.GetCursorPosition(cursor);
        auto before=rect(frame);
        Check(root.StartMoveOrResizeFrame(frame,MOVERESIZE_REASON_SCRIPT,cursor.x,cursor.y,point),"Resize point start");
        deliver(OS_INPUT_MOUSE_MOVE,0,144,324);auto after=rect(frame);
        const bool left=point==FRAMEPOINT_LEFT||point==FRAMEPOINT_TOPLEFT||point==FRAMEPOINT_BOTTOMLEFT;
        const bool right=point==FRAMEPOINT_RIGHT||point==FRAMEPOINT_TOPRIGHT||point==FRAMEPOINT_BOTTOMRIGHT;
        const bool top=point==FRAMEPOINT_TOP||point==FRAMEPOINT_TOPLEFT||point==FRAMEPOINT_TOPRIGHT;
        const bool bottom=point==FRAMEPOINT_BOTTOM||point==FRAMEPOINT_BOTTOMLEFT||point==FRAMEPOINT_BOTTOMRIGHT;
        Check(near(after.minX-before.minX,(left||point==FRAMEPOINT_CENTER)?NDCToDDCWidth(0.025f):0) &&
              near(after.maxX-before.maxX,(right||point==FRAMEPOINT_CENTER)?NDCToDDCWidth(0.025f):0) &&
              near(after.minY-before.minY,(bottom||point==FRAMEPOINT_CENTER)?NDCToDDCHeight(0.025f):0) &&
              near(after.maxY-before.maxY,(top||point==FRAMEPOINT_CENTER)?NDCToDDCHeight(0.025f):0),"Original edge/corner/center geometry");
        root.StopMoveOrResizeFrame();same(after,rect(frame));
    }
    place();frame->m_resizeRect={NDCToDDCHeight(0.2f),NDCToDDCWidth(0.2f),NDCToDDCHeight(0.4f),NDCToDDCWidth(0.4f)};
    deliver(OS_INPUT_MOUSE_MOVE);root.GetCursorPosition(cursor);root.StartMoveOrResizeFrame(frame,MOVERESIZE_REASON_SCRIPT,cursor.x,cursor.y,uint32_t(FRAMEPOINT_RIGHT));
    deliver(OS_INPUT_MOUSE_MOVE,0,640,336);Check(near(frame->GetWidth(),NDCToDDCWidth(0.4f)),"Maximum resize bound");
    deliver(OS_INPUT_MOUSE_MOVE,0,-640,336);Check(near(frame->GetWidth(),NDCToDDCWidth(0.2f)),"Minimum resize bound");root.StopMoveOrResizeFrame();
    frame->m_resizeRect={};place();
    // Layout modifier selection uses the original top-level stratum/hit search.
    root.m_moveResizeEnabled=1;frame->SetFrameFlag(0x100,1);frame->SetFrameFlag(0x200,1);
    deliver(OS_INPUT_KEY_DOWN,KEY_LALT,1,0);deliver(OS_INPUT_MOUSE_MOVE);deliver(OS_INPUT_MOUSE_DOWN,MOUSE_BUTTON_LEFT);
    Check(root.m_moveResizeFrame==frame && root.m_moveResizeReason==MOVERESIZE_REASON_LAYOUT && root.m_moveResizePoint==FRAMEPOINT_CENTER,"Alt layout move selection");
    deliver(OS_INPUT_MOUSE_UP,MOUSE_BUTTON_LEFT);deliver(OS_INPUT_KEY_UP,KEY_LALT,1,0);
    deliver(OS_INPUT_KEY_DOWN,KEY_LCONTROL,1,0);deliver(OS_INPUT_MOUSE_MOVE,0,248,360);deliver(OS_INPUT_MOUSE_DOWN,MOUSE_BUTTON_LEFT,248,360);
    Check(root.m_moveResizeFrame==frame && root.m_moveResizePoint==FRAMEPOINT_RIGHT,"Control layout edge selection");
    deliver(OS_INPUT_MOUSE_UP,MOUSE_BUTTON_LEFT,248,360);deliver(OS_INPUT_KEY_UP,KEY_LCONTROL,1,0);root.m_moveResizeEnabled=0;
    // Cancellation and consumer retirement never retain borrowed movement state.
    auto start=[&]{place();deliver(OS_INPUT_MOUSE_MOVE);root.GetCursorPosition(cursor);Check(root.StartMoveOrResizeFrame(frame,MOVERESIZE_REASON_SCRIPT,cursor.x,cursor.y,uint32_t(FRAMEPOINT_RIGHT)),"Cancellation movement start");};
    start();deliver(OS_INPUT_CAPTURE_CHANGED);Check(!root.m_moveResizeFrame,"Capture loss retained movement");
    start();deliver(OS_INPUT_FOCUS,0,0,0);Check(!root.m_moveResizeFrame,"Focus loss retained movement");
    start();frame->Hide();Check(!root.m_moveResizeFrame,"Hide retained movement");frame->Show();
    start();frame->DisableEvent(SIMPLE_EVENT_MOUSE);Check(!root.m_moveResizeFrame,"Disabled consumer retained movement");frame->EnableEvent(SIMPLE_EVENT_MOUSE,20);
    Reject([&]{root.StartMoveOrResizeFrame(reinterpret_cast<CSimpleFrame*>(uintptr_t(1)),1,0,0,uint32_t(0));});
    Reject([&]{root.StartMoveOrResizeFrame(frame,UINT32_MAX,0,0,uint32_t(0));});
    Reject([&]{root.StartMoveOrResizeFrame(frame,1,0,0,UINT32_MAX);});
    Reject([&]{root.StartMoveOrResizeFrame(frame,1,std::numeric_limits<float>::infinity(),0,uint32_t(0));});
    Reject([&]{frame->DragBy(0,0,UINT32_MAX);});
    bool foreign=false;std::thread thread([&]{try{root.StopMoveOrResizeFrame();}catch(const std::logic_error&){foreign=true;}});thread.join();Check(foreign,"Foreign movement accepted");
    // A virtual resize failure releases all traversal guards; cancellation and
    // subsequent movement still work. No exception or sanitizer is suppressed.
    start();frame->fail=true;bool threw=false;
    try{deliver(OS_INPUT_MOUSE_MOVE,0,144,336);}catch(const std::runtime_error&){threw=true;}
    frame->fail=false;Check(threw,"Movement resize failure swallowed");root.CancelMouseInput();
    start();deliver(OS_INPUT_MOUSE_MOVE,0,144,336);root.StopMoveOrResizeFrame();
    start();frame->defer=true;deliver(OS_INPUT_MOUSE_MOVE,0,144,336);root.OnLayerUpdate(0.01f);
    Check(!root.m_moveResizeFrame,"Deferred destruction retained moving Frame");
    auto* doomed=new CSimpleFrame(nullptr);doomed->SetAllPoints(&root,1);CLayoutFrame::ResizePending();root.GetCursorPosition(cursor);
    root.StartMoveOrResizeFrame(doomed,MOVERESIZE_REASON_SCRIPT,cursor.x,cursor.y,uint32_t(FRAMEPOINT_CENTER));delete doomed;
    Check(!root.m_moveResizeFrame,"Destruction retained movement");root.CancelMouseInput();
    std::cout<<"frame-title-layout-move-resize-nine-points-bounds-scale-cancellation-guards-pixels=passed\n";
}
