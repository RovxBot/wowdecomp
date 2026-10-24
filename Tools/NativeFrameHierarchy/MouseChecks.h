// Authored SDL pointer injection; physical input and widget acceptance are separate.
#include "Engine/Source/Event/CMouseEvent.h"
struct MouseRecord { int frame; uint32_t id; MouseEventData data; };
struct MouseFrame final : CSimpleFrame {
    int identity;
    std::vector<MouseRecord>& records;
    int enters=0, exits=0, starts=0, stops=0, drops=0;
    bool consumeWheel=false, defer=false, failRelease=false;
    MouseFrame(int id,std::vector<MouseRecord>& log):CSimpleFrame(nullptr),identity(id),records(log) {}
    void Record(const CMouseEvent& event) {
        Check(!event.m_param,"Mouse event param initialized");
        Reject([&]{Hide();});Reject([&]{SetParent(nullptr);});
        Reject([&]{Raise();});Reject([&]{SetFrameFlag(1,1);});
        Reject([&]{Frame::Native::RequireDestroyable();});
        records.push_back({identity,event.m_id,event.m_data});
        if(defer) Frame::Native::DestroyLater(this);
    }
    int32_t OnLayerTrackUpdate(const CMouseEvent& event) override {
        Record(event);return CSimpleFrame::OnLayerTrackUpdate(event);
    }
    int32_t OnLayerMouseDown(const CMouseEvent& event,const char* name) override {
        Record(event);return CSimpleFrame::OnLayerMouseDown(event,name);
    }
    int32_t OnLayerMouseUp(const CMouseEvent& event,const char* name) override {
        Record(event);
        if(failRelease) throw std::runtime_error("Authored pointer cancellation failure");
        return CSimpleFrame::OnLayerMouseUp(event,name);
    }
    int32_t OnLayerMouseWheel(const CMouseEvent& event) override {Record(event);return consumeWheel;}
    void OnLayerCursorEnter(int32_t motion) override {
        ++enters;Reject([&]{Frame::Native::RequireTreeMutation();});CSimpleFrame::OnLayerCursorEnter(motion);
    }
    void OnLayerCursorExit(int32_t motion,int32_t removed) override {
        ++exits;Reject([&]{Frame::Native::RequireDestroyable();});CSimpleFrame::OnLayerCursorExit(motion,removed);
    }
    void OnDragStart(const CMouseEvent&) override {++starts;}
    void OnDragStop(const CMouseEvent&) override {++stops;}
    void OnReceiveDrag(const CMouseEvent&) override {++drops;}
};
static GlContext* mouseContext=nullptr;
static bool throwMouseCallback=false, consumeMouseCallback=false;
static std::vector<MouseRecord> mouseCallbacks;
static int32_t MouseCallback(CMouseEvent* event) {
    mouseCallbacks.push_back({0,event->m_id,event->m_data});
    OSEVENT move{OS_INPUT_MOUSE_MOVE,{0,20,30,456}};
    Reject([&]{Frame::Native::DispatchInputEvent(*mouseContext,move);});
    Reject([&]{CSimpleTop::s_instance->OnLayerUpdate(0.01f);});
    Reject([&]{CSimpleTop::s_instance->OnLayerRender();});
    Reject([&]{Frame::Native::RequireDestroyable();});
    if(throwMouseCallback) throw std::runtime_error("Authored mouse callback failure");
    return consumeMouseCallback;
}
static void MouseEvents(GlContext& context,CSimpleTop& root) {
    auto deliver=[&](OSINPUT id,int value=0,int x=128,int y=360,int time=456) {
        return Frame::Native::DispatchInputEvent(context,OSEVENT{id,{value,x,y,time}});
    };
    auto pump=[&] {
        context.Window().Pump();OSEVENT event{};int selected=0;
        while(OsQueueGet(&event.id,&event.param[0],&event.param[1],&event.param[2],&event.param[3])) {
            if(Frame::Native::DispatchWindowEvent(context,event)) continue;
            if(Frame::Native::DispatchInputEvent(context,event)!=InputResult::Unsupported) ++selected;
        }
        return selected;
    };
    pump();root.CancelMouseInput();mouseContext=&context;mouseCallbacks.clear();
    root.m_mouseMoveCallback=MouseCallback;root.m_mouseButtonCallback=MouseCallback;
    std::vector<MouseRecord> records;
    auto* low=new MouseFrame(1,records);auto* high=new MouseFrame(2,records);
    auto place=[&](CSimpleFrame* frame,float x) {
        frame->SetSize(NDCToDDCWidth(0.3f),NDCToDDCHeight(0.4f));
        frame->SetPoint(FRAMEPOINT_BOTTOMLEFT,&root,FRAMEPOINT_BOTTOMLEFT,NDCToDDCWidth(x),NDCToDDCHeight(0.1f),1);
    };
    place(low,0.1f);place(high,0.1f);CLayoutFrame::ResizePending();
    low->EnableEvent(SIMPLE_EVENT_MOUSE,10);high->EnableEvent(SIMPLE_EVENT_MOUSE,20);
    low->EnableEvent(SIMPLE_EVENT_MOUSEWHEEL,10);high->EnableEvent(SIMPLE_EVENT_MOUSEWHEEL,20);
    Check(deliver(OS_INPUT_MOUSE_MOVE)==InputResult::Handled && root.m_mouseFocus==high,"Mouse priority/hit focus");
    Check(high->enters==1 && low->enters==0 && records.back().frame==2,"Mouse enter routing");
    Check(records.back().id==0x400500CA && records.back().data.m_eventTime==456,"Mouse move identity/time");
    C2Vector cursor;root.GetCursorPosition(cursor);
    Check(std::fabs(cursor.x-NDCToDDCWidth(0.2f))<0.00001f &&
          std::fabs(cursor.y-NDCToDDCHeight(0.25f))<0.00001f,"Logical mouse coordinate/Y conversion");
    const auto before=records.size();deliver(OS_INPUT_MOUSE_MOVE);
    Check(records.size()==before,"Stationary motion was redelivered");
    // Hit insets, pending-layout refresh and stratum ordering precede numeric priority.
    high->SetHitRectInsets(NDCToDDCWidth(0.15f),0,0,0);
    root.OnLayerUpdate(0.01f);
    deliver(OS_INPUT_MOUSE_MOVE,0,129);Check(root.m_mouseFocus==low && high->exits==1,"Hit inset did not expose lower frame");
    high->SetHitRectInsets(0,0,0,0);high->SetFrameStrata(FRAME_STRATA_BACKGROUND);
    deliver(OS_INPUT_MOUSE_MOVE,0,130);Check(root.m_mouseFocus==low,"Stratum did not outrank priority");
    high->SetFrameStrata(FRAME_STRATA_TOOLTIP);root.OnLayerUpdate(0.01f);
    Check(root.m_mouseFocus==high,"Stationary root update did not refresh mouse focus");
    records.clear();low->consumeWheel=true;
    Check(deliver(OS_INPUT_MOUSE_WHEEL,-240)==InputResult::Handled && records.size()==2 &&
          records[0].frame==2 && records[1].frame==1 && records[0].id==0x400500CD &&
          int32_t(records[0].data.m_wheelAmount)==-240,"Wheel order/sign/propagation");
    low->consumeWheel=false;
    Check(deliver(OS_INPUT_MOUSE_WHEEL,120)==InputResult::Unhandled,"Unhandled wheel swallowed");
    deliver(OS_INPUT_KEY_DOWN,KEY_LSHIFT,1,0);
    high->m_dragButtons=MOUSE_BUTTON_LEFT;
    Check(deliver(OS_INPUT_MOUSE_DOWN,MOUSE_BUTTON_LEFT)==InputResult::Handled && root.m_mouseCapture==high && high->m_isMouseDown,"Button capture/base drag state");
    Check(records.back().id==0x400500C8 && records.back().data.m_metaKeyState==1 && records.back().data.m_buttonState==1,"Button/modifier payload");
    deliver(OS_INPUT_KEY_UP,KEY_LSHIFT,1,0);
    // Captured tracking starts drag once; another hit frame receives the drop.
    place(low,0.6f);CLayoutFrame::ResizePending();
    deliver(OS_INPUT_MOUSE_MOVE,0,448,360);
    Check(root.m_mouseCapture==high && root.m_mouseFocus==low && high->starts==1,"Captured drag/focus transition");
    deliver(OS_INPUT_MOUSE_MOVE,0,449,360);Check(high->starts==1,"Drag started twice");
    deliver(OS_INPUT_MOUSE_DOWN,MOUSE_BUTTON_RIGHT,449);
    deliver(OS_INPUT_MOUSE_UP,MOUSE_BUTTON_RIGHT,449);
    Check(root.m_mouseCapture==high && records.back().data.m_buttonState==1,"Multibutton release retired capture early");
    deliver(OS_INPUT_MOUSE_UP,MOUSE_BUTTON_LEFT,449);
    Check(!root.m_mouseCapture && !high->m_isMouseDown && !high->m_isMouseDragging && high->stops==1 && low->drops==1,"Drag/drop/final release cleanup");
    // Host capture/focus loss clears state before calling consumers.
    deliver(OS_INPUT_MOUSE_DOWN,MOUSE_BUTTON_LEFT,449);
    low->failRelease=true;const auto exitsBefore=low->exits;bool cancelThrew=false;
    try{deliver(OS_INPUT_CAPTURE_CHANGED,0);}catch(const std::runtime_error&){cancelThrew=true;}
    low->failRelease=false;
    Check(cancelThrew && low->exits==exitsBefore+1,"Cancellation failure skipped cursor-exit cleanup");
    Check(!root.m_mouseCapture && !root.m_mouseFocus && !root.m_nativeButtonsDown && !low->m_isMouseDown,"Capture loss retained mouse state");
    deliver(OS_INPUT_MOUSE_MOVE,0,128);deliver(OS_INPUT_MOUSE_DOWN,MOUSE_BUTTON_LEFT);
    deliver(OS_INPUT_FOCUS,0,0,0);
    Check(!root.m_mouseFocus && !root.m_mouseCapture && !root.m_nativeButtonsDown && !high->m_isMouseDown,"Focus loss retained mouse state");
    deliver(OS_INPUT_MOUSE_MOVE,0,128);
    Check(root.m_mouseFocus==high,"Unchanged pointer position did not regain focus after cancellation");
    // Hide, disable and destruction remove borrowed pointers and queue entries.
    deliver(OS_INPUT_MOUSE_MOVE,0,129);deliver(OS_INPUT_MOUSE_DOWN,MOUSE_BUTTON_LEFT);
    high->Hide();Check(!root.m_mouseCapture && !root.m_mouseFocus && !high->m_isMouseDown,"Hide retained pointer capture");
    high->Show();root.OnLayerUpdate(0.01f);Check(root.m_mouseFocus==high,"Shown consumer focus refresh");
    high->DisableEvent(SIMPLE_EVENT_MOUSE);Check(!root.m_mouseFocus,"Disable retained focus");
    high->EnableEvent(SIMPLE_EVENT_MOUSE,20);root.OnLayerUpdate(0.01f);
    // Unselected event kinds still reject; original Lua mouse handlers execute.
    root.m_moveResizeEnabled=1;deliver(OS_INPUT_MOUSE_DOWN,MOUSE_BUTTON_LEFT);
    deliver(OS_INPUT_MOUSE_UP,MOUSE_BUTTON_LEFT);root.m_moveResizeEnabled=0;
    Reject([&]{high->EnableEvent(SIMPLE_EVENT_4,0);});
    CStatus scriptStatus;
    int32_t handler=FrameScript_CompileFunction("MouseHandlerFixture","return function(self,button) %s end",
        "assert(self==this and button=='LeftButton'); MouseLuaObserved=true",&scriptStatus);
    Check(handler>0 && scriptStatus.IsEmpty(),"Mouse handler fixture compilation");
    high->m_onMouseDown.m_luaRef=handler;
    CMouseEvent authored;authored.m_data.m_button=MOUSE_BUTTON_LEFT;
    high->CSimpleFrame::OnLayerMouseDown(authored,nullptr);
    high->CSimpleFrame::OnLayerMouseUp(authored,nullptr);
    high->m_onMouseDown.m_luaRef=0;FrameScript_ReleaseFunction(handler);
    auto* L=FrameScript_GetContext();lua_getglobal(L,"MouseLuaObserved");
    Check(lua_toboolean(L,-1),"Original base mouse handler did not execute Lua");lua_pop(L,1);
    lua_pushnil(L);lua_setglobal(L,"MouseLuaObserved");
    Reject([&]{deliver(OS_INPUT_MOUSE_DOWN,3);});Reject([&]{deliver(OS_INPUT_MOUSE_UP,-1);});
    Reject([&]{deliver(OS_INPUT_MOUSE_DOWN,32);});Reject([&]{deliver(OS_INPUT_MOUSE_MOVE,1);});
    EVENT_DATA_MOUSE invalid{};uint32_t raw=UINT32_MAX;
    std::memcpy(&invalid.m_mode,&raw,sizeof(raw));Reject([&]{CSimpleTop::OnMouseMove(&invalid,&root);});
    invalid={};invalid.m_x=std::numeric_limits<float>::infinity();Reject([&]{CSimpleTop::OnMouseWheel(&invalid,&root);});
    Reject([&]{CSimpleTop::OnMouseMove(nullptr,&root);});
    bool foreign=false;std::thread thread([&]{try{deliver(OS_INPUT_MOUSE_MOVE);}catch(const std::logic_error&){foreign=true;}});
    thread.join();Check(foreign,"Foreign mouse dispatch accepted");
    root.CancelMouseInput();
    // Actual SDL -> OsQueueGet -> original Frame delivery, all input authored.
    auto push=[&](SDL_Event& event){Check(SDL_PushEvent(&event),"SDL pointer injection failed");};
    const auto window=SDL_GetWindowID(context.Window().Handle());
    SDL_Event move{};move.type=SDL_EVENT_MOUSE_MOTION;move.motion.windowID=window;move.motion.x=128;move.motion.y=360;push(move);
    SDL_Event button{};button.type=SDL_EVENT_MOUSE_BUTTON_DOWN;button.button.windowID=window;button.button.button=SDL_BUTTON_LEFT;button.button.x=128;button.button.y=360;push(button);
    button.type=SDL_EVENT_MOUSE_BUTTON_UP;push(button);
    SDL_Event wheel{};wheel.type=SDL_EVENT_MOUSE_WHEEL;wheel.wheel.windowID=window;wheel.wheel.y=-2;wheel.wheel.mouse_x=128;wheel.wheel.mouse_y=360;push(wheel);
    const auto original=records.size();Check(pump()>=4,"SDL pointer events missing");
    Check(records.size()>original && root.m_mouseFocus==high && !root.m_mouseCapture &&
          records.back().id==0x400500CD && int32_t(records.back().data.m_wheelAmount)==-240,"SDL pointer sequence");
    Check(context.Window().RelativeMouse(true),"SDL relative mode unavailable");
    move.motion.xrel=4;move.motion.yrel=-3;push(move);pump();
    Check(std::any_of(mouseCallbacks.begin(),mouseCallbacks.end(),[](const MouseRecord& record){
        return record.id==0x400500CB && record.data.m_mode==MOUSE_MODE_RELATIVE && record.data.m_flags==2 &&
               record.data.m_mousePosition.x==NDCToDDCWidth(4) && record.data.m_mousePosition.y==NDCToDDCHeight(-3);
    }),"SDL relative motion/original DDC conversion missing");
    Check(context.Window().RelativeMouse(false),"SDL relative mode release failed");pump();
    // Throwing callbacks unwind the guard; deferral holds consumers until update.
    throwMouseCallback=true;bool threw=false;
    try{deliver(OS_INPUT_MOUSE_MOVE,0,131);}catch(const std::runtime_error&){threw=true;}
    throwMouseCallback=false;Check(threw,"Mouse callback exception swallowed");
    const auto recordsBefore=records.size();
    deliver(OS_INPUT_MOUSE_MOVE,0,131);
    Check(root.m_mouseFocus==high && records.size()>recordsBefore,"Unchanged-position mouse delivery did not recover");
    consumeMouseCallback=true;
    Check(deliver(OS_INPUT_MOUSE_MOVE_RELATIVE,0,4,-3)==InputResult::Handled,"Relative callback consumption ignored");
    consumeMouseCallback=false;
    deliver(OS_INPUT_MOUSE_DOWN,MOUSE_BUTTON_LEFT);delete high;
    Check(!root.m_mouseCapture && !root.m_mouseFocus,"Destroyed consumer retained pointers");
    low->defer=true;deliver(OS_INPUT_MOUSE_MOVE,0,448);root.OnLayerUpdate(0.01f);
    Check(!root.m_mouseFocus && !root.m_mouseCapture,"Deferred deletion retained mouse pointers");
    root.CancelMouseInput();root.m_mouseMoveCallback=nullptr;root.m_mouseButtonCallback=nullptr;mouseContext=nullptr;
    for(auto& queues:root.m_eventqueue) for(auto& queue:queues) Check(!queue.m_count,"Destroyed mouse queue entry");
    std::cout<<"sdl-frame-mouse-hit-priority-capture-wheel-relative-drag-guards-recovery=passed callbacks="<<mouseCallbacks.size()<<'\n';
    mouseCallbacks.clear();
}
