// Authored top-level composition and pointer routing; no physical input acceptance.
static void StackingEvents(GlContext& context,LegacyDevice& device,CSimpleTop& root,CSimpleFont& font) {
    std::vector<MouseRecord> records;
    auto place=[&](CSimpleFrame* frame,float x) {
        frame->SetSize(NDCToDDCWidth(0.5f),NDCToDDCHeight(0.4f));
        frame->SetPoint(FRAMEPOINT_BOTTOMLEFT,&root,FRAMEPOINT_BOTTOMLEFT,NDCToDDCWidth(x),NDCToDDCHeight(0.1f),1);
    };
    auto deliver=[&](OSINPUT id,int value=0,int x=128) {
        return Frame::Native::DispatchInputEvent(context,OSEVENT{id,{value,x,360,567}});
    };
    auto* low=new MouseFrame(3,records);auto* high=new MouseFrame(4,records);
    auto* hidden=new CSimpleFrame(nullptr);
    for(auto* frame:{static_cast<CSimpleFrame*>(low),static_cast<CSimpleFrame*>(high),hidden}) {
        frame->SetFrameStrata(FRAME_STRATA_DIALOG);place(frame,0.1f);
    }
    place(high,0.35f);low->SetFrameFlag(1,1);high->SetFrameFlag(1,1);
    hidden->SetFrameLevel(5,0);hidden->Hide();high->SetFrameLevel(10,0);
    low->EnableEvent(SIMPLE_EVENT_MOUSE,-1);high->EnableEvent(SIMPLE_EVENT_MOUSE,-1);
    auto text=[&](CSimpleFrame* parent,CImVector color) {
        auto* string=new CSimpleFontString(parent,2,1);
        string->SetSize(NDCToDDCWidth(0.2f),NDCToDDCHeight(0.1f));
        string->SetPoint(FRAMEPOINT_BOTTOMLEFT,&root,FRAMEPOINT_BOTTOMLEFT,NDCToDDCWidth(0.38f),NDCToDDCHeight(0.25f),1);
        string->SetFontObject(&font);string->SetText("AV",0);string->SetVertexColor(color);
    };
    text(low,CImVector(0,0,255,255));text(high,CImVector(0,255,0,255));
    auto green=Draw(device,root);auto* strata=root.m_strata[FRAME_STRATA_DIALOG];
    Check((low->m_frameFlags&0x10) && !(high->m_frameFlags&0x10),"Original top-level occlusion flags");
    Check(!root.RaiseFrame(hidden,1),"Non-top-level Frame was raised");
    Reject([&]{strata->FrameOccluded(reinterpret_cast<CSimpleFrame*>(uintptr_t(1)));});
    Reject([&]{root.m_strata[FRAME_STRATA_LOW]->FrameOccluded(low);});
    Reject([&]{low->SetFrameFlag(0x100,1);});
    // Consumed/throwing host callbacks cannot reorder Frames or reenter input.
    mouseContext=&context;root.m_mouseButtonCallback=MouseCallback;
    deliver(OS_INPUT_MOUSE_MOVE);Check(root.m_mouseFocus==low,"Exposed lower top-level focus");
    consumeMouseCallback=true;deliver(OS_INPUT_MOUSE_DOWN,MOUSE_BUTTON_LEFT);
    consumeMouseCallback=false;Check(low->m_level==0 && !root.m_mouseCapture,"Consumed click raised/captured Frame");
    root.CancelMouseInput();deliver(OS_INPUT_MOUSE_MOVE);
    throwMouseCallback=true;bool threw=false;
    try{deliver(OS_INPUT_MOUSE_DOWN,MOUSE_BUTTON_LEFT);}catch(const std::runtime_error&){threw=true;}
    throwMouseCallback=false;Check(threw && low->m_level==0 && !root.m_mouseCapture,"Callback failure reordered Frame");
    root.CancelMouseInput();root.m_mouseButtonCallback=nullptr;mouseContext=nullptr;
    // Actual click raising executes the original sparse compression, including
    // hidden registered Frames and its resumed (rather than dense-rank) scan.
    deliver(OS_INPUT_MOUSE_MOVE);deliver(OS_INPUT_MOUSE_DOWN,MOUSE_BUTTON_LEFT);
    Check(root.m_mouseCapture==low && low->m_level==6 && high->m_level==5 && hidden->m_level==1 && strata->m_topLevel==7,
          "Click raising/hidden sparse compression differs from original");
    auto red=Draw(device,root);
    Check(std::memcmp(red.data(),green.data(),red.size()*sizeof(CImVector))!=0,"Top-level raise did not change rendered order");
    Check(std::count_if(red.begin(),red.end(),[](const CImVector& p){return p.r>p.g+8;})>4 &&
          std::count_if(green.begin(),green.end(),[](const CImVector& p){return p.g>p.r+8;})>4,"Raised text color did not reach framebuffer");
    Equal(red,Draw(device,root));
    low->Raise();Check(low->m_level==6,"Unoccluded raise changed level");
    deliver(OS_INPUT_MOUSE_UP,MOUSE_BUTTON_LEFT);
    deliver(OS_INPUT_MOUSE_MOVE,0,448);Check(root.m_mouseFocus==high,"Exposed second top-level focus");
    deliver(OS_INPUT_MOUSE_DOWN,MOUSE_BUTTON_LEFT,448);
    Check(root.m_mouseCapture==high && high->m_level>low->m_level,"Second click did not reorder priority/capture");
    Equal(green,Draw(device,root));deliver(OS_INPUT_MOUSE_UP,MOUSE_BUTTON_LEFT,448);
    deliver(OS_INPUT_MOUSE_MOVE,0,256);Check(root.m_mouseFocus==high,"Raised level did not reorder mouse priority");
    // Touching edges and failed rectangles do not occlude; hidden Frames retain
    // levels during compression but are absent from the occluding strata lists.
    low->SetFrameLevel(0,0);high->SetFrameLevel(10,0);
    high->SetPoint(FRAMEPOINT_BOTTOMLEFT,low,FRAMEPOINT_BOTTOMRIGHT,0,0,1);root.OnLayerUpdate(0.01f);
    Check(!strata->FrameOccluded(low),"Touching rectangles were treated as occlusion");
    place(high,0.1f);root.OnLayerUpdate(0.01f);Check(strata->FrameOccluded(low),"Overlap was not occluded");
    high->Hide();Check(!strata->FrameOccluded(low),"Hidden Frame occluded lower top-level");
    high->Show();root.OnLayerUpdate(0.01f);
    high->m_layoutFlags&=~0x100u; // Explicit failed-layout fixture, not real display behavior.
    Check(!strata->FrameOccluded(low),"Invalid rectangle occluded lower top-level");
    root.CancelMouseInput();delete low;delete high;delete hidden;
    // Raising a child selects its nearest top-level ancestor and shifts children.
    auto* parent=new CSimpleFrame(nullptr);parent->SetFrameStrata(FRAME_STRATA_DIALOG);place(parent,0.1f);parent->SetFrameFlag(1,1);
    auto* child=new CSimpleFrame(parent);child->SetAllPoints(parent,1);
    CLayoutFrame::ResizePending();Check(!strata->FrameOccluded(parent),"Descendant occluded its own top-level ancestor");
    std::vector<CSimpleFrame*> fillers;
    for(int level=2;level<=128;++level) {
        auto* frame=new CSimpleFrame(nullptr);frame->SetFrameStrata(FRAME_STRATA_DIALOG);place(frame,0.1f);frame->SetFrameLevel(level,0);fillers.push_back(frame);
    }
    root.OnLayerUpdate(0.01f);
    Reject([&]{child->Raise();});
    Check(parent->m_level==0 && child->m_level==1 && strata->m_topLevel==129,"Rejected full-stratum raise mutated levels");
    delete fillers.back();fillers.pop_back();
    Reject([&]{child->Raise();}); // Parent 128 fits, but descendant 129 does not.
    Check(parent->m_level==0 && child->m_level==1 && strata->m_topLevel==129,"Rejected descendant overflow compressed lists");
    delete fillers.back();fillers.pop_back();child->Raise();
    Check(parent->m_level==127 && child->m_level==128,"Nearest top-level ancestor/child levels were not raised");
    for(auto* frame:fillers) delete frame;
    delete parent;root.CancelMouseInput();
    std::cout<<"frame-top-level-occlusion-click-raise-compression-hidden-child-bounds-pixels=passed\n";
}
