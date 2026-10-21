// Authored SDL composition/candidate injection, not physical IME acceptance.
#include "Engine/Source/Event/CImeEvent.h"
#include "Engine/Source/Os/OsIME.h"
struct ImeRecord {
    int frame;
    EVENT_DATA_IME data;
    std::string text, result;
    std::vector<std::string> candidates;
    uint32_t start, end, cursor, selection;
};
struct ImeFrame final : CSimpleFrame {
    int identity;
    GlContext& context;
    std::vector<ImeRecord>& records;
    bool consume=false, fail=false, defer=false;
    int characters=0;
    ImeFrame(int id,GlContext& owner,std::vector<ImeRecord>& log)
        :CSimpleFrame(nullptr),identity(id),context(owner),records(log) {}
    int32_t OnLayerChar(const CCharEvent&) override {++characters;return consume;}
    int32_t OnLayerIme(const CImeEvent& event) override {
        Check(event.m_id==0x40060068 && !event.m_param,"Original IME event identity");
        Check(event.m_data.m_codePage==65001,"Native IME must identify UTF-8");
        Reject([&]{Hide();});Reject([&]{DisableEvent(SIMPLE_EVENT_CHAR);});
        Reject([&]{Frame::Native::RequireDestroyable();});
        Reject([&]{CSimpleTop::s_instance->OnLayerUpdate(0.016f);});
        Reject([&]{CSimpleTop::s_instance->OnLayerRender();});
        Reject([&]{CSimpleTop::OnIme(&event.m_data,CSimpleTop::s_instance);});
        Reject([&]{Frame::Native::DispatchInputEvent(context,OSEVENT{OS_INPUT_IME,{1,0,0,0}});});
        char text[128],result[128];
        Check(OsIMEGetCompositionString(text,sizeof(text)) && OsIMEGetCompositionResult(result,sizeof(result)),"IME snapshot getter failed");
        ImeRecord record{identity,event.m_data,text,result,{},0,0,0,0};
        OsIMEGetClauseInfo(record.start,record.end,record.cursor);
        uint32_t page,pageSize,count;
        TSGrowableArray<OsIMECandidate> candidates;
        const auto hasCandidates=OsIMEGetCandidates(0,page,pageSize,count,record.selection,candidates);
        Check(bool(hasCandidates)==bool(count) && !page && pageSize==count,"Candidate page contract");
        for(uint32_t i=0;i<count;++i) record.candidates.emplace_back(candidates[i].m_text);
        if(!record.text.empty()) {
            char small[2];Check(!OsIMEGetCompositionString(small,sizeof(small)),"Short composition buffer accepted");
        }
        records.push_back(std::move(record));
        if(defer) Frame::Native::DestroyLater(this);
        if(fail) throw std::runtime_error("Authored IME consumer failure");
        return consume;
    }
};
static void ImeEvents(GlContext& context,CSimpleTop& root) {
    auto deliver=[&](int action,int flags=0,int unused=0) {
        return Frame::Native::DispatchInputEvent(context,OSEVENT{OS_INPUT_IME,{action,unused,flags,123}});
    };
    auto pump=[&] {
        context.Window().Pump();OSEVENT event{};int selected=0;
        while(OsQueueGet(&event.id,&event.param[0],&event.param[1],&event.param[2],&event.param[3])) {
            if(Frame::Native::DispatchWindowEvent(context,event)) continue;
            if(Frame::Native::DispatchInputEvent(context,event)!=InputResult::Unsupported) ++selected;
        }
        return selected;
    };
    pump();OsIMEEnable(1);
    std::vector<ImeRecord> records;
    auto* low=new ImeFrame(1,context,records);auto* high=new ImeFrame(2,context,records);
    low->EnableEvent(SIMPLE_EVENT_CHAR,10);high->EnableEvent(SIMPLE_EVENT_CHAR,20);
    Check(deliver(1)==InputResult::Unhandled && records.size()==2 && records[0].frame==2 && records[1].frame==1,"IME priority/propagation");
    low->consume=true;low->SetFrameStrata(FRAME_STRATA_TOOLTIP);records.clear();
    Check(deliver(1)==InputResult::Handled && records.size()==1 && records[0].frame==1,"IME stratum order");
    low->Hide();records.clear();high->consume=true;
    Check(deliver(1)==InputResult::Handled && records.size()==1 && records[0].frame==2,"Hidden IME consumer");
    high->DisableEvent(SIMPLE_EVENT_CHAR);records.clear();
    Check(deliver(1)==InputResult::Unhandled && records.empty(),"Disabled IME consumer");
    high->EnableEvent(SIMPLE_EVENT_CHAR,20);low->Show();low->SetFrameStrata(FRAME_STRATA_MEDIUM);
    low->consume=false;
    const auto before=records.size();
    for(int action:{-1,7}) Reject([&]{deliver(action);});
    for(int flags:{-1,0,8}) Reject([&]{deliver(2,flags);});
    Reject([&]{deliver(1,1);});Reject([&]{deliver(3,0,1);});
    EVENT_DATA_IME bad{1,0,0,1252};
    Reject([&]{CSimpleTop::OnIme(&bad,&root);});Reject([&]{CSimpleTop::OnIme(nullptr,&root);});
    Reject([&]{CSimpleTop::OnIme(&bad,nullptr);});
    Check(deliver(0)==InputResult::Unsupported && records.size()==before,"Unsupported language/invalid IME invoked consumers");
    bool foreign=false;std::thread thread([&]{try{deliver(1);}catch(const std::logic_error&){foreign=true;}});
    thread.join();Check(foreign,"Foreign IME dispatch accepted");
    const auto window=SDL_GetWindowID(context.Window().Handle());
    auto push=[&](SDL_Event& event){event.window.windowID=window;Check(SDL_PushEvent(&event),"SDL IME injection failed");};
    auto edit=[&](const char* text,int start,int length) {
        SDL_Event event{};event.type=SDL_EVENT_TEXT_EDITING;event.edit.text=text;
        event.edit.start=start;event.edit.length=length;push(event);
    };
    auto candidates=[&](const char* const* values,int count,int selected) {
        SDL_Event event{};event.type=SDL_EVENT_TEXT_EDITING_CANDIDATES;
        event.edit_candidates.candidates=values;event.edit_candidates.num_candidates=count;
        event.edit_candidates.selected_candidate=selected;push(event);
    };
    const char* values[]={"\xe4\xb8\xad","\xe4\xb8\xad\xe6\x96\x87"};
    records.clear();
    // Pump all authored SDL events before dequeuing: every Os snapshot must
    // survive later composition/candidate/commit changes in the same pump.
    edit(values[0],0,1);edit("\xe4\xb8\xad\xf0\x9f\x98\x80",1,1);
    candidates(values,2,1);candidates(values,1,0);candidates(nullptr,0,-1);
    SDL_Event commit{};commit.type=SDL_EVENT_TEXT_INPUT;commit.text.text=values[1];push(commit);
    Check(pump()>=8 && records.size()==8,"SDL IME action count");
    const int actions[]={1,2,2,3,4,5,2,6};
    for(size_t i=0;i<records.size();++i) Check(records[i].frame==2 && records[i].data.m_action==actions[i],"SDL IME order/consumption");
    Check(records[1].text==values[0] && records[1].start==0 && records[1].end==1,"First composition snapshot");
    Check(records[2].text=="\xe4\xb8\xad\xf0\x9f\x98\x80" && records[2].start==1 && records[2].end==2 && records[2].cursor==1,"Supplementary scalar clause positions");
    Check(records[3].candidates==std::vector<std::string>({values[0],values[1]}) && records[3].selection==1,"Candidate open snapshot");
    Check(records[4].candidates==std::vector<std::string>({values[0]}) && records[4].selection==0 && records[5].candidates.empty(),"Candidate change/close snapshots");
    Check(records[6].data.m_param==4 && records[6].text.empty() && records[6].result==values[1] && records[7].result==values[1],"Composition result/end snapshot");
    Check(!high->characters && !low->characters,"IME commit duplicated as character input");
    // Empty edit cancellation and focus loss retire composition/candidate state.
    records.clear();edit(values[0],-1,200);edit("",0,0);pump();
    Check(records.size()==4 && records[1].start==0 && records[1].end==1 && records.back().data.m_action==6 && records.back().text.empty(),"Composition cancellation/clause bounds");
    records.clear();edit(values[0],0,1);
    SDL_Event focus{};focus.type=SDL_EVENT_WINDOW_FOCUS_LOST;push(focus);pump();
    Check(records.size()==3 && records.back().data.m_action==6 && records.back().text.empty(),"Focus loss composition cancellation");
    records.clear();candidates(values,2,0);push(focus);pump();
    Check(records.size()==2 && records[0].data.m_action==3 && records.back().data.m_action==6 && records.back().candidates.empty(),"Focus loss candidate-only cancellation");
    // Callback failure must unwind guards; deferred destruction stays safe.
    records.clear();high->fail=true;bool threw=false;
    try{deliver(1);}catch(const std::runtime_error&){threw=true;}
    high->fail=false;Check(threw && deliver(6)==InputResult::Handled,"IME callback failure recovery");
    high->defer=true;deliver(1);root.OnLayerUpdate(0.016f);
    records.clear();Check(deliver(1)==InputResult::Unhandled && records.size()==1 && records[0].frame==1,"Deferred IME consumer destruction");
    delete low;
    auto* plain=new CSimpleFrame(nullptr);plain->EnableEvent(SIMPLE_EVENT_CHAR,0);
    Check(deliver(1)==InputResult::Unhandled,"Original empty base IME handler");delete plain;
    OsIMEEnable(0);
    std::cout<<"sdl-frame-ime-composition-candidates-commit-focus-priority-guards-recovery=passed snapshots=8\n";
}
