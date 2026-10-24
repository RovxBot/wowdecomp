// Authored SDL event injection, distinct from physical keyboard/IME acceptance.
#include "Engine/Source/Event/CCharEvent.h"
#include "Engine/Source/Event/CKeyEvent.h"
#include "Engine/Source/Event/CFocusEvent.h"
using InputResult = Frame::Native::InputResult;
struct KeyRecord { int frame; uint32_t id; EVENT_DATA_KEY data; };
struct KeyboardFrame final : CSimpleFrame {
    int identity;
    std::vector<KeyRecord>& records;
    std::vector<uint32_t> scalars;
    bool consume=false, defer=false;
    KeyboardFrame(int id,std::vector<KeyRecord>& log):CSimpleFrame(nullptr),identity(id),records(log) {}
    int32_t Record(const CKeyEvent& event) {
        Reject([&]{Hide();});
        Reject([&]{EnableEvent(SIMPLE_EVENT_CHAR,0);});
        Reject([&]{Frame::Native::RequireDestroyable();});
        records.push_back({identity,event.m_id,event.m_data});
        if(defer) Frame::Native::DestroyLater(this);
        return consume;
    }
    int32_t OnLayerKeyDown(const CKeyEvent& event) override {return Record(event);}
    int32_t OnLayerKeyUp(const CKeyEvent& event) override {return Record(event);}
    int32_t OnLayerKeyDownRepeat(const CKeyEvent& event) override {return Record(event);}
    int32_t OnLayerChar(const CCharEvent& event) override {
        Check(event.m_id==0x40060067 && !event.m_param,"Original character event identity");
        scalars.push_back(uint32_t(event.m_data.m_ch));return consume;
    }
};
static GlContext* keyboardContext=nullptr;
static bool throwKeyCallback=false;
static int keyCallbacks=0,focusCallbacks=0;
static void KeyCallback(const CKeyEvent& event) {
    Check(event.m_id==0x40060064 && !event.m_param,"Original key callback identity");
    ++keyCallbacks;
    OSEVENT key{OS_INPUT_KEY_DOWN,{KEY_B,1,0,0}};
    Reject([&]{Frame::Native::DispatchInputEvent(*keyboardContext,key);});
    Reject([&]{CSimpleTop::OnKeyDown(&event.m_data,CSimpleTop::s_instance);});
    Reject([&]{CSimpleTop::s_instance->OnLayerUpdate(0.016f);});
    Reject([&]{CSimpleTop::s_instance->OnLayerRender();});
    Reject([&]{Frame::Native::RequireDestroyable();});
    if(throwKeyCallback) throw std::runtime_error("Authored key callback failure");
}
static void FocusCallback(const CFocusEvent& event) {
    Check(event.m_id==0x40040065 && !event.m_param,"Original focus callback identity");
    Check(event.m_data.m_focus==0 || event.m_data.m_focus==1,"Invalid focus payload");
    ++focusCallbacks;
    Reject([&]{Frame::Native::RequireTreeMutation();});
}
static void KeyboardEvents(GlContext& context,CSimpleTop& root) {
    auto deliver=[&](OSINPUT id,int value,int repeat=1,int time=123) {
        return Frame::Native::DispatchInputEvent(context,OSEVENT{id,{value,repeat,0,time}});
    };
    auto pump=[&] {
        context.Window().Pump();OSEVENT event{};int selected=0;
        while(OsQueueGet(&event.id,&event.param[0],&event.param[1],&event.param[2],&event.param[3])) {
            if(Frame::Native::DispatchWindowEvent(context,event)) continue;
            if(Frame::Native::DispatchInputEvent(context,event)!=InputResult::Unsupported) ++selected;
        }
        return selected;
    };
    pump();keyboardContext=&context;keyCallbacks=focusCallbacks=0;
    root.m_keyDownCallback=KeyCallback;root.m_focusChangedCallback=FocusCallback;
    std::vector<KeyRecord> records;
    auto* low=new KeyboardFrame(1,records);auto* high=new KeyboardFrame(2,records);
    low->EnableEvent(SIMPLE_EVENT_KEY,10);high->EnableEvent(SIMPLE_EVENT_KEY,20);
    low->EnableEvent(SIMPLE_EVENT_CHAR,10);high->EnableEvent(SIMPLE_EVENT_CHAR,20);
    // Priority within a stratum, then original key-down capture/up/repeat routing.
    Check(deliver(OS_INPUT_KEY_DOWN,KEY_A)==InputResult::Unhandled,"Unhandled key propagation");
    Check(records.size()==2 && records[0].frame==2 && records[1].frame==1,"Original event priority order");
    Check(records[0].id==0x40060064 && records[0].data.m_time==123,"Original key payload/time");
    high->consume=true;records.clear();
    deliver(OS_INPUT_KEY_UP,KEY_A);
    Check(deliver(OS_INPUT_KEY_DOWN,KEY_A)==InputResult::Handled && root.m_keydownCapture[KEY_A]==high,"Key capture missing");
    Check(deliver(OS_INPUT_KEY_DOWN,KEY_A)==InputResult::Handled && records.back().id==0x40060065,"Captured repeat routing");
    high->DisableEvent(SIMPLE_EVENT_KEY);Check(!root.m_keydownCapture[KEY_A],"Disable left key capture");
    Check(deliver(OS_INPUT_KEY_UP,KEY_A)==InputResult::Unhandled,"Disabled capture received release");
    high->EnableEvent(SIMPLE_EVENT_KEY,20);
    deliver(OS_INPUT_KEY_DOWN,KEY_A);high->Hide();
    Check(!root.m_keydownCapture[KEY_A],"Hide left key capture");high->Show();deliver(OS_INPUT_KEY_UP,KEY_A);
    // Stratum wins over numeric priority; capture survives an original level move.
    high->SetFrameStrata(FRAME_STRATA_TOOLTIP);
    deliver(OS_INPUT_KEY_DOWN,KEY_A);high->SetFrameLevel(1,0);
    Check(deliver(OS_INPUT_KEY_UP,KEY_A)==InputResult::Handled && records.back().frame==2 && records.back().id==0x40060066,"Moved capture release");
    // Highest native key indices previously exceeded the original 787-slot array.
    deliver(OS_INPUT_KEY_DOWN,KEY_RSUPER);Check(root.m_keydownCapture[KEY_RSUPER]==high,"Native Super capture");
    deliver(OS_INPUT_KEY_UP,KEY_RSUPER);Check(!root.m_keydownCapture[KEY_RSUPER],"Native Super release");
    deliver(OS_INPUT_KEY_UP,KEY_PRINTSCREEN);Check(!root.m_keydownCapture[KEY_PRINTSCREEN],"PrintScreen fallback retained capture");
    deliver(OS_INPUT_KEY_DOWN,KEY_LCONTROL);
    deliver(OS_INPUT_KEY_DOWN,KEY_B);Check(records.back().data.m_metaKeyState==(1u<<KEY_LCONTROL),"Original modifier bits");
    deliver(OS_INPUT_KEY_UP,KEY_B);deliver(OS_INPUT_KEY_UP,KEY_LCONTROL);
    Reject([&]{high->EnableEvent(UINT32_MAX,0);});Reject([&]{high->DisableEvent(UINT32_MAX);});
    Reject([&]{high->EnableEvent(SIMPLE_EVENT_JOYSTICK,0);});
    Reject([&]{root.RegisterForEvent(high,SIMPLE_EVENT_KEY,0,0);});
    Reject([&]{deliver(OS_INPUT_KEY_DOWN,-1);});Reject([&]{deliver(OS_INPUT_KEY_UP,KEY_LAST);});
    Reject([&]{deliver(OS_INPUT_KEY_DOWN,KEY_A,0);});Reject([&]{deliver(OS_INPUT_CHAR,0xd800);});
    Reject([&]{deliver(OS_INPUT_CHAR,0x110000);});Reject([&]{deliver(OS_INPUT_FOCUS,2);});
    Check(deliver(OS_INPUT_IME,0)==InputResult::Unsupported && deliver(OS_INPUT_CLOSE,0)==InputResult::Unsupported,"Unsupported input consumed");
    bool foreign=false;std::thread thread([&]{try{deliver(OS_INPUT_CHAR,'A');}catch(const std::logic_error&){foreign=true;}});
    thread.join();Check(foreign,"Foreign input dispatch accepted");
    // Actual SDL queue translation, with authored injected events.
    auto push=[&](SDL_Event& event){Check(SDL_PushEvent(&event),"SDL keyboard injection failed");};
    const auto window=SDL_GetWindowID(context.Window().Handle());
    SDL_Event event{};event.type=SDL_EVENT_KEY_DOWN;event.key.windowID=window;event.key.key=SDLK_A;event.key.scancode=SDL_SCANCODE_A;
    push(event);event.key.repeat=true;push(event);
    SDL_Event text{};text.type=SDL_EVENT_TEXT_INPUT;text.text.windowID=window;text.text.text="A\xc3\xa9\xf0\x9f\x98\x80";push(text);
    SDL_Event focus{};focus.type=SDL_EVENT_WINDOW_FOCUS_LOST;focus.window.windowID=window;push(focus);
    const auto before=records.size();const auto callbacksBefore=keyCallbacks;
    Check(pump()>=7,"SDL queue did not deliver key/repeat/text/release/focus");
    Check(records.size()==before+3 && records[before].id==0x40060064 && records[before+1].id==0x40060065 && records.back().id==0x40060066,"SDL capture sequence");
    Check(keyCallbacks==callbacksBefore+1 && focusCallbacks>=1,"SDL callback counts");
    Check(high->scalars==std::vector<uint32_t>({'A',0xe9,0x1f600}) && low->scalars.empty(),"SDL Unicode scalar delivery/consumption");
    Check(!root.m_keydownCapture[KEY_A] && !root.m_nativeKeysDown[KEY_A] && !root.m_nativeMetaKeys,"Focus left held input");
    // Exceptions unwind traversal guards; subsequent delivery and destruction work.
    throwKeyCallback=true;bool threw=false;
    try{deliver(OS_INPUT_KEY_DOWN,KEY_C);}catch(const std::runtime_error&){threw=true;}
    throwKeyCallback=false;Check(threw,"Authored callback failure did not propagate");
    deliver(OS_INPUT_KEY_UP,KEY_C);deliver(OS_INPUT_KEY_DOWN,KEY_C);
    Check(root.m_keydownCapture[KEY_C]==high,"Input did not recover after callback failure");
    delete high;Check(!root.m_keydownCapture[KEY_C],"Destroyed consumer retained capture");
    deliver(OS_INPUT_KEY_UP,KEY_C);
    low->consume=true;low->defer=true;deliver(OS_INPUT_KEY_DOWN,KEY_D);
    root.OnLayerUpdate(0.016f);Check(!root.m_keydownCapture[KEY_D],"Deferred deletion retained key capture");
    deliver(OS_INPUT_KEY_UP,KEY_D);
    for(auto& queues:root.m_eventqueue) for(auto& queue:queues) Check(!queue.m_count,"Destroyed input queue entry");
    auto* plain=new CSimpleFrame(nullptr);
    plain->EnableEvent(SIMPLE_EVENT_KEY,0);plain->EnableEvent(SIMPLE_EVENT_CHAR,0);
    Check(deliver(OS_INPUT_KEY_DOWN,KEY_A)==InputResult::Unhandled && deliver(OS_INPUT_CHAR,'A')==InputResult::Unhandled,"Empty base input handlers");
    deliver(OS_INPUT_KEY_UP,KEY_A);delete plain;
    root.m_keyDownCallback=nullptr;root.m_focusChangedCallback=nullptr;keyboardContext=nullptr;
    std::cout<<"sdl-frame-key-text-focus-priority-capture-repeat-guards-recovery=passed callbacks="<<keyCallbacks<<" scalars=3\n";
}
